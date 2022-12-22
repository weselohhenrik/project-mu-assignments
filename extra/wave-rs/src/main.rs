use std::{fs::File, env, io::ErrorKind};

use wav::header;

fn AMDF(data: &Vec<i16>, tau: usize) -> f64 {
    let mut sum: f64 = 0.0;
    //let range = data.len() - tau;
    for i in 0..(data.len() - tau -1) {
        let sample_a = data[i] as f64;
        let sample_b = data[i+tau] as f64;
        sum += f64::abs(sample_a - sample_b);
    }

    sum/(data.len() as f64 - tau as f64)
}

fn local_minima(vals: &Vec<f64>) -> Vec<usize> {
    let mut minima: Vec<usize> = Vec::new();

    for i in 1..vals.len()-1 {
        if vals[i] < vals[i+1] && vals[i] < vals[i-1] {
            minima.push(i);
        }
    }
    minima
}

fn detect_frequency_amdf(data: &Vec<i16>, sample_rate: u32) -> f64 {
    let mut amdf_vals: Vec<f64> = Vec::new();
    for i in 0..data.len() {
        amdf_vals.push(AMDF(data, i));
    }

    let samples = local_minima(&amdf_vals);
    let period = samples[1] - samples[0];
    (sample_rate as f64)/(period as f64)
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let file_name = String::from("./") + &args[1];
    let wav_file_result = File::open(file_name);

    let mut wav_file = match wav_file_result {
        Ok(file) => file,
        Err(error) => {
            panic!("Could not find file!");
        }
    };

    let (header, data) = wav::read(&mut wav_file).unwrap();
    let duration: usize = (header.sampling_rate as usize)/1000 * 50;

    let data_vec = data.as_sixteen().unwrap();
    let sample = &data_vec[0..duration].to_vec();

    let sample_rate = header.sampling_rate;
    let frequency = detect_frequency_amdf(sample, sample_rate);

    println!("Frequency: {:?}", frequency);

}
