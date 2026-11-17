use rust_codegen_multifile_test::{
    SecondaryModule, TertiaryModule, register_secondary, register_tertiary,
};

#[derive(Default)]
struct Secondary;

impl SecondaryModule for Secondary {
    fn ping(&self, value: f64) -> Result<f64, String> {
        Ok(value)
    }
}

register_secondary!(Secondary::default);

#[derive(Default)]
struct Tertiary;

impl TertiaryModule for Tertiary {
    fn pong(&self, value: f64) -> Result<f64, String> {
        Ok(value)
    }
}

register_tertiary!(Tertiary::default);
