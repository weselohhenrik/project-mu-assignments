use std::{process};

use egui::Key;
use jack::{AudioOut, Port, NotificationHandler, ProcessHandler};

const EPS: f32 = 0.001;
const ATK: f32 = 0.002;
const REL: f32 = 0.002;

enum Message {
    StatusChanged(f32)
}

struct ExciteApp {
    status: f32,
    sender: crossbeam::channel::Sender<Message>
}

impl eframe::App for ExciteApp {
    fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
        frame.set_window_size(egui::Vec2 { x: 400.0, y: 400.0 });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Press A");

            if ctx.input().key_pressed(Key::A) {
                self.status = 1.0;
                self.sender.send(Message::StatusChanged(self.status)).unwrap();
            }

            if ctx.input().key_released(Key::A) {
                self.status = 0.0;
                self.sender.send(Message::StatusChanged(self.status)).unwrap();
            }
        });
    }
}

enum State {
    Idle,
    Attack,
    Release
}

struct ExciteEngine {
    status: f32,
    receiver: crossbeam::channel::Receiver<Message>,
    port_out: Port<AudioOut>,
    state: State,
    target: f32,
    pole: f32,
    gate_old: f32,
}

impl ExciteEngine {
    fn new(status: f32, receiver: crossbeam::channel::Receiver<Message>, port_out: Port<AudioOut>) -> ExciteEngine {
        let ee = ExciteEngine {
            status,
            receiver,
            port_out,
            state: State::Idle,
            target: 0.0,
            pole: 0.0,
            gate_old: 0.0
        };
        ee
    }

    fn ratio2pole(t: f32, ratio: f32, sr: usize) -> f32 {
        ratio.powf(1.0/(t* sr as f32))
    }
}

impl ProcessHandler for ExciteEngine {
    fn process(&mut self, client: &jack::Client, ps: &jack::ProcessScope) -> jack::Control {
        let sr = client.sample_rate();
        let buffer_out = self.port_out.as_mut_slice(ps);

        while let Ok(message) = self.receiver.try_recv() {
            match message {
                Message::StatusChanged(val) => {
                    self.status = val;
                }
            }
        }

        //let mut gate_old = 0.0;
        let mut ar = 0.0;
        for o in buffer_out.iter_mut() {
            let rand: f32 = rand::random();

            if self.status > self.gate_old {
                self.state = State::Attack;
                self.target = 1.0 + EPS;
                self.pole = ExciteEngine::ratio2pole(ATK, EPS /(self.target), sr);
            } else if self.status < self.gate_old {
                self.state = State::Release;
                self.target = -EPS;
                self.pole = ExciteEngine::ratio2pole(REL, EPS / (1.0 + EPS), sr);
            }
            self.gate_old = self.status;
            ar = (1.0 - self.pole) * self.target + self.pole * ar;

            match self.state {
                State::Idle => {
                    ar = 0.0;
                }
                State::Attack => {
                    if ar >= 1.0 {
                        ar = 1.0;
                        self.state = State::Release;
                        self.target = -EPS;
                        self.pole = ExciteEngine::ratio2pole(REL, EPS / (1.0 + EPS), sr);
                    }
                }
                State::Release => {
                    if ar <= 0.0 {
                        ar = 0.0;
                        self.state = State::Idle;
                    }
                }
            }

            *o = ar * rand;
        }
        
        jack::Control::Continue
    }
}


struct ShutdownHandler {}
impl NotificationHandler for ShutdownHandler {
    fn shutdown(&mut self, _status: jack::ClientStatus, _reason: &str) {
        process::exit(1);
    }
}

fn main() {
    println!("Hello, excite");

    let (client, status) = jack::Client::new(
        "excite",
        jack::ClientOptions::NO_START_SERVER
    ).unwrap();

    if !status.is_empty() {
        eprintln!("Failed to open client");
        process::exit(1);
    }

    let port_out = client.register_port("out", AudioOut::default()).unwrap();

    let (sender, receiver) = crossbeam::channel::unbounded();

    let excite_engine = ExciteEngine::new(0.0, receiver, port_out);
    let excite_app = ExciteApp {
        status: 0.0,
        sender
    };

    let sh = ShutdownHandler{};
    let client_active = client.activate_async(sh, excite_engine).unwrap();

    let native_options = eframe::NativeOptions::default();
    eframe::run_native("Excite", native_options, Box::new(|_| Box::new(excite_app)));

    client_active.deactivate().unwrap();
}