use jack::AudioOut;
use jack::AudioIn;
use std::f32::consts::TAU;
use std::{io, io::Read, process};

//const FREQ: f32 = 440.0;
const AMP: f32 = 0.1;
const TBL_LEN: usize = 512;

enum Message {
  IncRadius,
  DecRadius,
  IncCenterX,
  DecCenterX,
  IncCenterY,
  DecCenterY
}

struct OscApp {
  radius: f32,
  center_x: f32,
  center_y: f32,
  sender: crossbeam::channel::Sender<Message>
}

impl eframe::App for OscApp {
  fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
      frame.set_window_size(egui::Vec2{x: 500.0, y: 500.0});

      egui::CentralPanel::default().show(ctx, |ui| {
        ui.label("Radius");
        if ui.button("Increase").clicked() {
          self.radius += 0.1;
          self.sender.send(Message::IncRadius).unwrap();
        }
        if ui.button("Decrease").clicked() {
          self.radius -= 0.1;
          self.sender.send(Message::DecRadius).unwrap();
        }
        ui.label("Center X");
        if ui.button("Increase").clicked() {
          self.center_x += 0.1;
          self.sender.send(Message::IncCenterX).unwrap();
        }
        if ui.button("Decrease").clicked() {
          self.center_x -= 0.1;
          self.sender.send(Message::DecCenterX).unwrap();
        }
        ui.label("Center Y");
        if ui.button("Increase").clicked() {
          self.center_y += 0.1;
          self.sender.send(Message::IncCenterY).unwrap();
        }
        if ui.button("Decrease").clicked() {
          self.center_y -= 0.1;
          self.sender.send(Message::DecCenterY).unwrap();
        }
    });
  } 
}

fn map(val: f32, x0: f32, x1: f32, y0: f32, y1: f32) -> f32 {
  y0 + (y1 - y0) * (val - x0) / (x1 - x0)
}
struct WaveTerrain {
  terrain: Vec<Vec<f32>>,
  radius: f32,
  center_x: f32,
  center_y: f32
}

impl WaveTerrain {
  fn new() -> WaveTerrain {

    let mut wave_terrain = WaveTerrain {
      terrain: vec![vec![]],
      radius: 0.5,
      center_x: 0.0,
      center_y: 0.0
    };

    for y in 0..TBL_LEN {
      let mut temp: Vec<f32> = Vec::new();
      for x in 0..TBL_LEN {
        let _x = map(x as f32, -(TBL_LEN as f32), TBL_LEN as f32, -1.0, 1.0);
        let _y = map(y as f32, -(TBL_LEN as f32), TBL_LEN as f32, -1.0, 1.0);
        let value = _x.sin() * _y.sin() * (_x - _y) * (_x - 1.0)*(_x + 1.0) * (_y - 1.0)*(_y + 1.0);
        temp.push(value);
      }
      wave_terrain.terrain.push(temp);
    }
    wave_terrain
  }

  fn eval(&self, mut x: f32, mut y: f32) -> f32 {
    while x > 1.0 {
      x -= 1.0;
    }
    while y > 1.0 {
      y -= 1.0;
    }

    let integer_i = map(x, -1.0, 1.0, 0.0, TBL_LEN as f32) as usize;
    let i_dx = integer_i;

    let integer_j = map(y, -1.0, 1.0, 0.0, TBL_LEN as f32) as usize;
    let j_dx = integer_j;

    let mut fractional_i = x * (TBL_LEN as f32);
    fractional_i = fractional_i - integer_i as f32;

    let mut fractional_j = y * (TBL_LEN as f32);
    fractional_j = fractional_j - integer_j as f32;

    //println!("i_dx: {:?}, j_dx: {:?}", integer_i, integer_j);

    let v00 = self.terrain[i_dx][j_dx];
    let v10 = self.terrain[(i_dx + 1) % TBL_LEN][j_dx];
    let v01 = self.terrain[i_dx][(j_dx + 1) % TBL_LEN];
    let v11 = self.terrain[(i_dx + 1) % TBL_LEN][(j_dx + 1) % TBL_LEN];

    //println!("v00: {:?}, v10: {:?}, v01: {:?}, v11: {:?}", v00, v10, v01, v11);

    interpolate(v00, v10, v01, v11, fractional_i, fractional_j)
  }
}

fn interpolate(v00: f32, v10: f32, v01: f32, v11: f32, i_fr: f32, j_fr: f32) -> f32 {
  let u0 = (1.0 - i_fr)* v00 + i_fr*v10;
  let u1 = (1.0 - i_fr)*v01 + i_fr*v11;

  let out = (1.0-j_fr)*u0 + j_fr*u1;
  out
}

struct WaveTable {
  tbl: Vec<f32>
}

impl WaveTable {
  fn new() -> WaveTable {
    let step = 1.0 / TBL_LEN as f32;

    let mut table = WaveTable {
      tbl: vec![]
    };
    
    for i in 0..TBL_LEN {
      table.tbl.push(TAU * (i as f32) * step)
    }
    table
  }

  fn eval(&self, phs: f32) -> f32 {
    let mut fractional = phs  * (TBL_LEN as f32);
    let integer = fractional.floor() as usize;

    fractional = fractional - integer as f32;

    let x0 = self.tbl[integer];
    let x1 = self.tbl[(integer + 1) % TBL_LEN];

    (1.0 - fractional)*x0 + fractional*x1
  }

}

struct OscDriver {
  terrain: WaveTerrain,
  table: WaveTable,
  phs: f32,
  phs_in: jack::Port<AudioIn>,
  freq_in: jack::Port<AudioIn>,
  out: jack::Port<AudioOut>,
  receiver: crossbeam::channel::Receiver<Message>
}

impl jack::ProcessHandler for OscDriver {
  fn process(
    &mut self,
    client: &jack::Client,
    ps: &jack::ProcessScope,
  ) -> jack::Control {
    let out = self.out.as_mut_slice(ps);

    while let Ok(command) = self.receiver.try_recv() {
      match command {
          Message::IncRadius => {
            self.terrain.radius += 0.1;
          }
          Message::DecRadius => {
            self.terrain.radius -= 0.1;
          }
          Message::IncCenterX => {
            self.terrain.center_x += 0.1;
          }
          Message::DecCenterX => {
            self.terrain.center_x -= 0.1;
          }
          Message::IncCenterY => {
            self.terrain.center_y += 0.1;
          }
          Message::DecCenterY => {
            self.terrain.center_y -= 0.1;
          }
      }
    }



    let phs_buffer = self.phs_in.as_slice(ps);
    for (i, o) in out.iter_mut().enumerate() {
      self.phs = phs_buffer[i];
      let x = self.terrain.radius * (self.phs * TAU).cos() + self.terrain.center_x;
      let y = self.terrain.radius * (self.phs * TAU).sin() + self.terrain.center_y;
      *o = AMP * self.terrain.eval(x, y);
    }

    jack::Control::Continue
  }
}

struct ShutdownHandler;
impl jack::NotificationHandler for ShutdownHandler {
  fn shutdown(&mut self, _status: jack::ClientStatus, _reason: &str) {
    die("jack server is down, exiting...")
  }
}

fn main() {
  // initialize table
  let tbl = WaveTable::new();
  let terrain = WaveTerrain::new();

  // open client
  let (client, status) = jack::Client::new(
    "terrain", jack::ClientOptions::NO_START_SERVER
    ).unwrap_or_die(
        "fail to open client"
    );
  if !status.is_empty() {
    die("fail to open client");
  }

  // init sender receiver
  let (sender, receiver) = crossbeam::channel::unbounded();


  // register output audio port
  let port_out = client
    .register_port("out", AudioOut::default())
    .unwrap_or_die("fail to register port");

  let phs_in_port = client
        .register_port("phs", AudioIn::default())
        .unwrap_or_die("fail to register poirt");
  let freq_in_port = client
        .register_port("freq", AudioIn::default())
        .unwrap_or_die("fail to register poirt");
  // activate client
  let osc_driver = OscDriver {
    phs: 0.0,
    terrain,
    table: tbl,
    phs_in: phs_in_port,
    freq_in: freq_in_port,
    out: port_out,
    receiver
  };
  let client_active = client
    .activate_async(ShutdownHandler, osc_driver)
    .unwrap_or_die("fail to activate client");

  // Initialize App
  let osc_app = OscApp {
    radius: 0.5,
    center_x:  0.0,
    center_y: 0.0,
    sender
  };

  let native_options = eframe::NativeOptions::default();
  eframe::run_native("Terrain", native_options, Box::new(|_| Box::new(osc_app)));

  client_active
    .deactivate()
    .unwrap_or_die("fail to deactivate client");
}

// below are some shenanigans to deal with rust's horrible
// panic messages. feel free to ignore them

fn die(msg: &str) -> ! {
  eprintln!("{}", msg);
  process::exit(1);
}

trait UnwrapDie<T> {
  fn unwrap_or_die(self, msg: &str) -> T;
}
impl<T, E> UnwrapDie<T> for Result<T, E> {
  fn unwrap_or_die(self, msg: &str) -> T {
    match self {
      Ok(t) => t,
      Err(_) => die(msg),
    }
  }
}