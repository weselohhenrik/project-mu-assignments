use jack::AudioOut;
use jack::AudioIn;
use std::f32::consts::TAU;
use std::{process};

//const FREQ: f32 = 440.0;
const AMP: f32 = 0.1;
const TBL_LEN: usize = 512;

enum Message {
  Radius(f32),
  CenterX(f32),
  CenterY(f32)
}

struct OscApp {
  radius: f32,
  last_radius: f32,
  center_x: f32,
  last_center_x: f32,
  center_y: f32,
  last_center_y: f32,
  sender: crossbeam::channel::Sender<Message>
}

impl eframe::App for OscApp {
  fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
      frame.set_window_size(egui::Vec2{x: 500.0, y: 500.0});

      egui::CentralPanel::default().show(ctx, |ui| {
        ui.label("Radius");
        ui.add(egui::Slider::new(&mut self.radius, 0.0..=0.99).text("radius"));

        ui.label("Center X");
        ui.add(egui::Slider::new(&mut self.center_x, -0.49..=0.49).text("center x"));
        
        ui.label("Center Y");
        ui.add(egui::Slider::new(&mut self.center_y, -0.49..=0.49).text("center y").vertical());

        if self.radius != self.last_radius {
          self.sender.send(Message::Radius(self.radius)).unwrap();
          self.last_radius = self.radius;
        }
        if self.center_x != self.last_center_x {
          self.sender.send(Message::CenterX(self.center_x)).unwrap();
          self.last_center_x = self.center_x;
        }
        if self.center_y != self.last_center_y {
          self.sender.send(Message::CenterY(self.center_y)).unwrap();
          self.last_center_y = self.center_y;
        }

    });

        
  
  } 
}

fn map(val: f32, x0: f32, x1: f32, y0: f32, y1: f32) -> f32 {
  y0 + (y1 - y0) * (val - x0) / (x1 - x0)
}
struct WaveTerrain {
  terrain: Vec<f32>,
  radius: f32,
  center_x: f32,
  center_y: f32
}

impl WaveTerrain {
  fn new() -> WaveTerrain {

    let mut wave_terrain = WaveTerrain {
      terrain: vec![0.0; TBL_LEN*TBL_LEN],
      radius: 0.5,
      center_x: 0.0,
      center_y: 0.0
    };

    for y in 0..TBL_LEN {
      for x in 0..TBL_LEN {
        let _x = map(x as f32, -(TBL_LEN as f32), TBL_LEN as f32, -1.0, 1.0);
        let _y = map(y as f32, -(TBL_LEN as f32), TBL_LEN as f32, -1.0, 1.0);
        let value = _x.sin() * _y.sin() * (_x - _y) * (_x - 1.0)*(_x + 1.0) * (_y - 1.0)*(_y + 1.0);
        wave_terrain.terrain[x + y * TBL_LEN] = value;
      }
    }
    wave_terrain
  }

  fn eval(&self, mut x: f32, mut y: f32) -> f32 {
    while x >= 1.0 {
      x -= 1.0;
    }
    while y >= 1.0 {
      y -= 1.0;
    }

    let (i_dx, fractional_i) = index_and_fractional(y);
    let (j_dx, fractional_j) = index_and_fractional(x);

    let j10 = (j_dx + 1) % TBL_LEN;
    let i01 = (i_dx + 1) % TBL_LEN;

    let mut index_a = j_dx + TBL_LEN * i_dx;
    let mut index_b = j10 + TBL_LEN * i_dx;
    let mut index_c = j_dx + TBL_LEN * i01;
    let mut index_d = j10 + TBL_LEN * i01;

    if index_a >= TBL_LEN * TBL_LEN {
      index_a -= TBL_LEN * TBL_LEN;
    }
    
    if index_b >= TBL_LEN * TBL_LEN {
      index_b -= TBL_LEN * TBL_LEN;
    }
    if index_c >= TBL_LEN * TBL_LEN {
      index_c -= TBL_LEN * TBL_LEN;
    }
    if index_d >= TBL_LEN * TBL_LEN {
      index_d -= TBL_LEN * TBL_LEN;
    }

    let v00 = self.terrain[index_a];
    let v10 = self.terrain[index_b];
    let v01 = self.terrain[index_c];
    let v11 = self.terrain[index_d];

    interpolate(v00, v10, v01, v11, fractional_i, fractional_j)
  }
}

fn index_and_fractional(number: f32) -> (usize, f32) {
  let mut index = map(number, -1.0, 1.0, 0.0, TBL_LEN as f32).trunc() as usize;

  if index >= TBL_LEN * TBL_LEN {
    index -= TBL_LEN * TBL_LEN;
  }

  let fractional = (number * TBL_LEN as f32).fract();
  (index, fractional)
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
  mod_freq: f32,
  mod_phs: f32,
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
          Message::Radius(value) => {
            self.terrain.radius = value;
          }
          Message::CenterX(value) => {
            self.terrain.center_x = value;
          }
          Message::CenterY(value) => {
            self.terrain.center_y = value;
          }
      }
    }


    let sr = client.sample_rate() as f32;
    let phs_buffer = self.phs_in.as_slice(ps);
    for (i, o) in out.iter_mut().enumerate() {
      //self.terrain.radius += 0.1*((TAU * self.mod_phs).sin());
      self.phs = phs_buffer[i];
      let x = self.terrain.radius * (self.phs * TAU).cos() + self.terrain.center_x;
      let y = self.terrain.radius * (self.phs * TAU).sin() + self.terrain.center_y;
      *o = AMP * self.terrain.eval(x, y);

      self.mod_phs += self.mod_freq/sr;
      while self.mod_phs >= 1.0 {
        self.mod_phs -= 1.0;
      }
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
    mod_freq: 3.0,
    mod_phs: 0.0,
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
    last_radius: 0.5,
    center_x:  0.0,
    last_center_x: 0.0,
    center_y: 0.0,
    last_center_y: 0.0,
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

#[cfg(test)]
mod tests {
    use crate::index_and_fractional;

  #[test]
  fn indexes_1() {
    let (index, fractional) = index_and_fractional(0.5);
    println!("index: {:?}, fractional: {:?}", index, fractional);
    assert!(index < 256)
  }

  #[test]
  fn indexes_2() {
    let (index, fractional) = index_and_fractional(0.7);
    println!("index: {:?}, fractional: {:?}", index, fractional);
    assert!(index < 256)
  }
  #[test]
  fn indexes_3() {
    let (index, fractional) = index_and_fractional(1.0);
    println!("index: {:?}, fractional: {:?}", index, fractional);
    assert!(index < 256)
  }
}