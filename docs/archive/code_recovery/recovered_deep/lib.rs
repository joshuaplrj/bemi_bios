// BEMI DBT Library — Dynamic Binary Translation Pipeline
// std is always required (iced-x86 dependency + Vec/String in translator).
// no_std support is planned for a future refactor (TODO Phase 5).

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
