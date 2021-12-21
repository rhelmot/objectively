#![feature(ptr_metadata)]
#![feature(cursor_remaining)]
#![feature(try_reserve)]
#![feature(result_into_ok_or_err)]
#![feature(type_alias_impl_trait)]

mod builtins;
mod gcell;
mod interpreter;
mod object;
mod gdict;

fn main() {
    println!("Hello, world!");
}
