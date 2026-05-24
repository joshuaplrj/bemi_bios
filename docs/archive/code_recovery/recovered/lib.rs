#![cfg_attr(not(feature = "x8088"), no_std)]

#[cfg(feature = "x8088")]
extern crate std;

pub mod decoder;
pub mod translator;
pub mod executor;
pub mod optimizer;
pub mod codegen;
pub mod ir;

#[cfg(feature = "x8088")]
pub mod x8088;

pub use codegen::*;
pub use decoder::*;
pub use translator::*;
pub use executor::*;
pub use optimizer::*;
pub use ir::*;
