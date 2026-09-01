#![no_std]
#![no_main]

use uefi::{entry, println, Status};
use core::panic::PanicInfo;

#[entry]
fn main() -> Status {
    uefi::helpers::init().unwrap();
    println!("Hello from UEFI!");
    loop {
        unsafe { core::arch::asm!("cli; hlt"); }
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    println!("ouuu shiiiii");
    loop {}
}