use jack::AudioOut;
use std::f32::consts::TAU;
use std::process;

//const FREQ: f32 = 440.0;

struct PhsApp {
  freq: f32,
  last_freq: f32,
  sender: crossbeam::channel::Sender<f32>
}

impl eframe::App for PhsApp {
  fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
      frame.set_window_size(egui::Vec2{x: 500.0, y: 500.0});

      egui::CentralPanel::default().show(ctx, |ui| {
        ui.add(egui::Slider::new(&mut self.freq, 50.0..=600.0).text("frequency"));
        if self.freq != self.last_freq {
          self.sender.send(self.freq).unwrap();
          self.last_freq = self.freq;
        }
    });
  } 
}
struct PhsDriver {
  command_receiver: crossbeam::channel::Receiver<f32>,
  phs: f32,
  freq: f32,
  last_freq: f32,
  phs_out: jack::Port<AudioOut>,
  freq_out: jack::Port<AudioOut>
}


impl jack::ProcessHandler for PhsDriver {
  fn process(
    &mut self,
    client: &jack::Client,
    ps: &jack::ProcessScope,
  ) -> jack::Control {
    let sr = client.sample_rate() as f32;
    let phs_buffer = self.phs_out.as_mut_slice(ps);
    let freq_buffer = self.freq_out.as_mut_slice(ps);
    let freq_result = self.command_receiver.try_recv();

    let freq = match freq_result {
        Ok(f) => { f }
        Err(_) => { 0.0 }
    };

    if freq != 0.0 {
      self.last_freq = freq;
    }
    self.freq = self.last_freq;

    for (i, o) in phs_buffer.iter_mut().enumerate() {
      *o = (TAU * self.phs).sin();
      freq_buffer[i] = self.freq;
      //println!("phs: {:?}", self.phs);
      self.phs += self.freq / sr;
      while self.phs >= 1.0 {
        self.phs -= 1.0;
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
  // open client
  let (client, status) = jack::Client::new(
    "phs", jack::ClientOptions::NO_START_SERVER
    ).unwrap_or_die(
        "fail to open client"
    );
  if !status.is_empty() {
    die("fail to open client");
  }

  // register output audio port
  let phs_out_port = client
    .register_port("phs", AudioOut::default())
    .unwrap_or_die("fail to register port");

  let freq_out_port = client
    .register_port("freq", AudioOut::default())
    .unwrap_or_die("fail to register port");

  let (sender, receiver) = crossbeam::channel::unbounded();
  let phs_driver = PhsDriver {
    phs: 0.0,
    freq: 220.0,
    last_freq: 220.0,
    phs_out: phs_out_port,
    freq_out: freq_out_port,
    command_receiver: receiver
  };

  let phs_app = PhsApp {
    freq: 220.0,
    last_freq: 220.0,
    sender: sender
  };

  // activate client
  let client_active = client
    .activate_async(ShutdownHandler, phs_driver)
    .unwrap_or_die("fail to activate client");

  let native_options = eframe::NativeOptions::default();
  eframe::run_native("PHS", native_options, Box::new(|_| Box::new(phs_app)));

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