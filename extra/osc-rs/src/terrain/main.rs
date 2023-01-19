use jack::AudioOut;
use jack::AudioIn;
use std::f32::consts::TAU;
use std::{io, io::Read, process};

const FREQ: f32 = 440.0;
const AMP: f32 = 0.1;

// this is what you get when static mut variables are unsafe
struct Sine {
  phs: f32,
  in_port: jack::Port<AudioIn>,
  out: jack::Port<AudioOut>,
}
impl jack::ProcessHandler for Sine {
  fn process(
    &mut self,
    client: &jack::Client,
    ps: &jack::ProcessScope,
  ) -> jack::Control {
    let sr = client.sample_rate() as f32;
    let out = self.out.as_mut_slice(ps);

    let in_buffer = self.in_port.as_slice(ps);
    for o in out.iter_mut() {
      *o = AMP * (TAU * self.phs).sin();

      self.phs += FREQ / sr;
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
    "terrain", jack::ClientOptions::NO_START_SERVER
    ).unwrap_or_die(
        "fail to open client"
    );
  if !status.is_empty() {
    die("fail to open client");
  }

  // register output audio port
  let port_out = client
    .register_port("out", AudioOut::default())
    .unwrap_or_die("fail to register port");

  let port_in = client
        .register_port("in", AudioIn::default())
        .unwrap_or_die("fail to register poirt");


  // activate client
  let sine = Sine {
    phs: 0.0,
    in_port: port_in,
    out: port_out,
  };
  let client_active = client
    .activate_async(ShutdownHandler, sine)
    .unwrap_or_die("fail to activate client");

  // idle the main thread
  //   clean signal handling in rust requires external
  //   crates, so we will ignore them here
  io::stdin().read_exact(&mut [0]).unwrap_or(()); // a.k.a. getchar

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