#![feature(ptr_metadata)]
#![feature(cursor_remaining)]
#![feature(generic_associated_types)]
#![feature(result_into_ok_or_err)]

mod builtins;
mod gcell;
mod interpreter;
mod object;
mod gdict;

fn main() {
    println!("Hello, world!");
}
