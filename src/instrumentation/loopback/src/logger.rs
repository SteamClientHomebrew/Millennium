use log::{Log, Metadata, Record};

struct MepLogger;

impl Log for MepLogger {
    fn enabled(&self, _: &Metadata) -> bool {
        true
    }

    fn log(&self, r: &Record) {
        let level = match r.level() {
            log::Level::Error => "error",
            log::Level::Warn => "warn",
            _ => "info",
        };
        crate::ipc::send_log(level, &r.args().to_string());
    }

    fn flush(&self) {}
}

pub fn init() {
    let _ = log::set_logger(Box::leak(Box::new(MepLogger)));
    log::set_max_level(log::LevelFilter::Info);
}
