use std::cell::{UnsafeCell};
use shredder::{Scan, Scanner};
use shredder::marker::{GcSafe, GcDrop};
use std::ops::CoerceUnsized;
use std::sync::atomic::{AtomicBool, Ordering};

pub struct GCell<T: ?Sized>(UnsafeCell<T>);

pub struct GCellOwner(());

// The singular lock owner. Can be taken and put into a mutex
impl GCellOwner {
    pub fn make() -> GCellOwner {
        static CREATED: AtomicBool = AtomicBool::new(false);
        let created = CREATED.swap(true, Ordering::Relaxed);
        if created {
            panic!("GCellOwner::make() was called multiple times")
        } else {
            GCellOwner(())
        }
    }
}

impl <T> GCell<T> {
    pub fn new(value: T) -> Self {
        GCell(UnsafeCell::new(value))
    }
}

impl<T: ?Sized> GCell<T> {
    pub fn ro(&self, _lock: &GCellOwner) -> &T {
        unsafe {
            &*self.0.get()
        }
    }
    pub fn rw(&self, _lock: &mut GCellOwner) -> &mut T {
        unsafe { &mut *self.0.get() }
    }
}

unsafe impl<T: ?Sized> GcSafe for GCell<T> where T: GcSafe {}
unsafe impl<T: ?Sized> GcDrop for GCell<T> where T: GcDrop {}

unsafe impl <T: ?Sized> Scan for GCell<T> where T: Scan {
    fn scan(&self, scanner: &mut Scanner<'_>) {
        unsafe {
            (*self.0.get()).scan(scanner)
        }
    }
}

unsafe impl <T: ?Sized + Send + Sync> Sync for GCell<T> {}

impl <T, U> CoerceUnsized<GCell<U>> for GCell<T> where T: CoerceUnsized<U> {}