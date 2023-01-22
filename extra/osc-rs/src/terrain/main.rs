use jack::AudioOut;
use jack::AudioIn;
use std::f32::consts::TAU;
use std::{io, io::Read, process};

//const FREQ: f32 = 440.0;
const AMP: f32 = 0.1;
const TBL_LEN: usize = 512;

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
  tbl: WaveTable,
  phs: f32,
  phs_in: jack::Port<AudioIn>,
  freq_in: jack::Port<AudioIn>,
  out: jack::Port<AudioOut>,
}
impl jack::ProcessHandler for OscDriver {
  fn process(
    &mut self,
    client: &jack::Client,
    ps: &jack::ProcessScope,
  ) -> jack::Control {
    let sr = client.sample_rate() as f32;
    let out = self.out.as_mut_slice(ps);

    let phs_buffer = self.phs_in.as_slice(ps);
    let freq_buffer = self.freq_in.as_slice(ps);
    for (i, o) in out.iter_mut().enumerate() {
      //*o = AMP * (TAU * phs_buffer[i]).sin();
      *o = AMP * self.tbl.eval(phs_buffer[i]);

      self.phs += freq_buffer[i] / sr;
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
  // initialize table
  let tbl = WaveTable::new();

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

  let phs_in_port = client
        .register_port("phs", AudioIn::default())
        .unwrap_or_die("fail to register poirt");
  let freq_in_port = client
        .register_port("freq", AudioIn::default())
        .unwrap_or_die("fail to register poirt");
  // activate client
  let osc_driver = OscDriver {
    phs: 0.0,
    tbl,
    phs_in: phs_in_port,
    freq_in: freq_in_port,
    out: port_out,
  };
  let client_active = client
    .activate_async(ShutdownHandler, osc_driver)
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