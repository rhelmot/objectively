use std::{
    cell::UnsafeCell,
    ops::{Deref, DerefMut},
    sync::atomic::{AtomicBool, Ordering},
};

use shredder::{GcGuard, marker::{GcDrop, GcSafe}, Scan, Scanner};

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

impl<T> GCell<T> {
    pub fn new(value: T) -> Self {
        GCell(UnsafeCell::new(value))
    }
}

pub struct Ref<'a, R: Deref<Target = GCell<T>>, T: ?Sized>(pub R, pub &'a GCellOwner);
pub struct RefMut<'a, R: Deref<Target = GCell<T>>, T: ?Sized>(pub R, pub &'a mut GCellOwner);

pub(crate) type GcRef<'a, T> = Ref<'a, GcGuard<'a, GCell<T>>, T>;
pub(crate) type GcRefMut<'a, T> = RefMut<'a, GcGuard<'a, GCell<T>>, T>;

impl<T: ?Sized> GCell<T> {
    pub fn ro<'a>(&'a self, _lock: &'a GCellOwner) -> &'a T {
        unsafe { &*self.0.get() }
    }
    pub fn rw<'a>(&'a self, _lock: &'a mut GCellOwner) -> &'_ mut T {
        unsafe { &mut *self.0.get() }
    }
}

impl<'a, R, T: ?Sized> Deref for Ref<'_, R, T>
where
    R: Deref<Target = GCell<T>>,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.0.ro(self.1)
    }
}

impl<T> GCell<T> where T: Immutable {
    pub fn uncell(self) -> T {
        self.0.into_inner()
    }
    pub fn unwrap_ref(&self) -> &T {
        unsafe { &*self.0.get() }
    }
}

impl<T> Deref for GCell<T> where T: Immutable {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.0.get() }
    }
}

impl<'a, R, T: ?Sized> Deref for RefMut<'_, R, T>
where
    R: Deref<Target = GCell<T>>,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.0.ro(self.1)
    }
}

impl<'a, R, T: ?Sized> DerefMut for RefMut<'_, R, T>
where
    R: Deref<Target = GCell<T>>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0.rw(self.1)
    }
}

unsafe impl<T: ?Sized> GcSafe for GCell<T> where T: GcSafe {}
unsafe impl<T: ?Sized> GcDrop for GCell<T> where T: GcDrop {}

unsafe impl<T: ?Sized> Scan for GCell<T>
where
    T: Scan,
{
    fn scan(&self, scanner: &mut Scanner<'_>) {
        unsafe { (*self.0.get()).scan(scanner) }
    }
}

unsafe impl<T: ?Sized + Send + Sync> Sync for GCell<T> {}

pub unsafe trait Immutable {}
