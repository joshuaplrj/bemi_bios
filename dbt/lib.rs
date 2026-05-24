#![cfg_attr(not(feature = "x8088"), no_std)]

#[cfg(feature = "x8088")]
extern crate std;

pub mod ir;
pub mod optimizer;
pub mod executor;
pub mod codegen;

#[cfg(feature = "x8088")]
pub mod decoder;

#[cfg(feature = "x8088")]
pub mod translator;

#[cfg(feature = "x8088")]
pub mod x8088;

pub use ir::*;
pub use optimizer::*;
pub use executor::*;
pub use codegen::*;
