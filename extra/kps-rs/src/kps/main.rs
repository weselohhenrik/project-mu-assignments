use std::{process};

use jack::*;

const MAX_DELAY: usize = 1<<24;

enum AudioControlMessage {
    Feedback(f32),
    Frequency(f32)
}

struct KpsApp {
    feedback: f32,
    last_feedback: f32,
    frequency: f32,
    last_frequency: f32,
    sender: crossbeam::channel::Sender<AudioControlMessage>
}
impl KpsApp {
    fn new(feedback: f32, frequency: f32, sender: crossbeam::channel::Sender<AudioControlMessage>) -> KpsApp {
        let app = KpsApp {
            feedback,
            last_feedback: feedback,
            frequency,
            last_frequency: frequency,
            sender
        };
        app
    }
}
impl eframe::App for KpsApp {
    fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
        frame.set_window_size(egui::Vec2 { x: 400.0, y: 400.0 });

        egui::CentralPanel::default().show(ctx, |ui| {

            ui.add(egui::Slider::new(&mut self.frequency, 130.0..=600.0).text("frequency"));
            ui.add(egui::Slider::new(&mut self.feedback, 0.5..=1.0).text("feedback"));

            if self.frequency != self.last_frequency {
                self.sender.send(AudioControlMessage::Frequency(self.frequency)).unwrap();
                self.last_frequency = self.frequency;
            }
            if self.feedback != self.last_feedback {
                self.sender.send(AudioControlMessage::Feedback(self.feedback)).unwrap();
                self.last_feedback = self.feedback;
            }
        });
    }
}

struct RingBuffer {
    buffer: Vec<f32>,
    read_pointer: usize,
    write_pointer: usize
}

impl RingBuffer {
    fn new() -> RingBuffer {
        let buffer = vec![0.0; MAX_DELAY];

        let ring_buffer = RingBuffer { buffer , read_pointer: 0, write_pointer: 0 };
        ring_buffer
    }

    fn eval(&self ,pos: f32) -> f32 {
        let integer = pos.trunc() as usize;
        let fr = pos.fract();

        let x0 = self.buffer[integer % MAX_DELAY];
        let x1 = self.buffer[(integer + 1) % MAX_DELAY];

        (1.0-fr)*x0 + fr*x1
    }
}

struct ShutdownHandler{}
impl jack::NotificationHandler for ShutdownHandler {
    fn shutdown(&mut self, _status: ClientStatus, _reason: &str) {
        process::exit(0);
    }
}

struct KpsEngine {
    feedback: f32,
    frequency: f32,
    port_in: jack::Port<AudioIn>,
    port_out: jack::Port<AudioOut>,
    receiver: crossbeam::channel::Receiver<AudioControlMessage>,
    ring_buffer: RingBuffer,
    delay_mem: f32,
    feedback_mem: f32
}


impl KpsEngine {
    fn new(
        feedback: f32,
        frequency: f32,
        port_in: jack::Port<AudioIn>,
        port_out: jack::Port<AudioOut>,
        receiver: crossbeam::channel::Receiver<AudioControlMessage>,
    ) -> KpsEngine {
        let ring_buffer = RingBuffer::new();

        let engine = KpsEngine {
            feedback,
            frequency,
            port_in,
            port_out,
            receiver,
            ring_buffer,
            delay_mem: 0.0,
            feedback_mem: 0.0,
        };
        engine
    }

}

impl ProcessHandler for KpsEngine {
    fn process(&mut self,
        client: &jack::Client,
        ps: &jack::ProcessScope
    ) -> Control {
        let sr = client.sample_rate();
        let buffer_in = self.port_in.as_slice(ps);
        let buffer_out = self.port_out.as_mut_slice(ps);

        // TODO: receive messages
        while let Ok(command) =  self.receiver.try_recv() {
            match command {
                AudioControlMessage::Feedback(val) => {
                    self.feedback = val;
                }
                AudioControlMessage::Frequency(val) => {
                    self.frequency = val;
                }
            }
        }

        let delay = (sr as f32)/self.frequency;

        for (i, o) in buffer_out.iter_mut().enumerate() {
            self.ring_buffer.read_pointer = self.ring_buffer.write_pointer - self.delay_mem as usize;
            let mut rp = self.ring_buffer.read_pointer as f32;
            if rp < 0.0 {
                rp += MAX_DELAY as f32;
            }
            /*println!("read_pointer: {:?}", self.ring_buffer.read_pointer);
            println!("read_pointer as f32: {:?}", rp);
            println!("write_pointer : {:?}", self.ring_buffer.write_pointer);
            println!("");*/

            //delay_tick
            self.delay_mem = 0.0001* delay + 0.9999 * self.delay_mem;
            // feedback tick
            self.feedback_mem = 0.001* self.feedback + 0.999 * self.feedback_mem;

            *o = buffer_in[i] + self.feedback_mem * (
                0.5*self.ring_buffer.eval(rp)+ 0.5*self.ring_buffer.eval(rp + 1.0)
            );

            self.ring_buffer.buffer[self.ring_buffer.write_pointer] = *o;

            self.ring_buffer.write_pointer += 1;
            if self.ring_buffer.write_pointer >= MAX_DELAY {
                self.ring_buffer.write_pointer -= MAX_DELAY;
            }
        }
        
        
        jack::Control::Continue
    }
}

fn main() {
    println!("Hello, kps");

    let (client, status) = jack::Client::new(
        "kps",
        jack::ClientOptions::NO_START_SERVER
    ).unwrap();

    if !status.is_empty() {
        eprintln!("Failed to open client");
        process::exit(1);
    }

    let port_in = client.register_port("in", AudioIn::default()).unwrap();
    let port_out = client.register_port("out", AudioOut::default()).unwrap();

    let (sender, receiver) = crossbeam::channel::unbounded();


    let kps_engine = KpsEngine::new(0.7, 220.0, port_in, port_out, receiver);
    let kps_app = KpsApp::new(0.7, 220.0, sender);
    let sh = ShutdownHandler {};

    let client_active = client.activate_async(sh, kps_engine).unwrap();

    let native_options = eframe::NativeOptions::default();
    eframe::run_native("KPS", native_options, Box::new(|_| Box::new(kps_app)));

    client_active.deactivate().unwrap();
}