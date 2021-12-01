use core::mem;
use std::{
    convert::TryInto,
    io::{Cursor, Read},
};

use byteorder::ReadBytesExt;
use leb128;
use num_enum::TryFromPrimitive;

use crate::{
    gcell::GCellOwner,
    object::{
        BytesObject, DictObject, ExceptionObject, Object, Result, TupleObject, G_BYTES, GcGCellExt,
        MEMORYERROR_INST, IntObject, FloatObject, G_NONE, G_TRUE, G_FALSE,
    },
};

enum StepResult {
    Normal,
    TryPush(i32),
    TryPop,
}

fn vec_reserve<T>(v: &mut Vec<T>, n: usize) -> Result<()> {
    v.try_reserve(n).map_err(|_| MEMORYERROR_INST.clone())
}

fn fallible_push<T>(vec: &mut Vec<T>, val: T) -> Result<()> {
    vec_reserve(vec, 1)?;
    vec.push(val);
    Ok(())
}

fn fallible_pop<T>(vec: &mut Vec<T>) -> Result<T> {
    if let Some(r) = vec.pop() {
        Ok(r)
    } else {
        return Err(ExceptionObject::runtime_error("Stack underflow")?);
    }
}

#[allow(non_camel_case_types)]
#[derive(TryFromPrimitive)]
#[repr(u8)]
enum Opcode {
    ERROR = 0,
    ST_SWAP = 1,
    ST_POP = 2,
    ST_DUP = 3,
    ST_DUP2 = 4,
    LIT_BYTES = 10,
    LIT_INT = 11,
    LIT_FLOAT = 12,
    LIT_SLICE = 13,
    LIT_NONE = 14,
    LIT_TRUE = 15,
    LIT_FALSE = 16,
    TUPLE_0 = 17,
    TUPLE_1 = 18,
    TUPLE_2 = 19,
    TUPLE_3 = 20,
    TUPLE_4 = 21,
    TUPLE_N = 22,
    CLOSURE = 23,
    CLOSURE_BIND = 24,
    EMPTY_DICT = 25,
    CLASS = 26,
    GET_ATTR = 40,
    SET_ATTR = 41,
    DEL_ATTR = 42,
    GET_ITEM = 43,
    SET_ITEM = 44,
    DEL_ITEM = 45,
    GET_LOCAL = 46,
    SET_LOCAL = 47,
    DEL_LOCAL = 48,
    LOAD_ARGS = 49,
    JUMP = 60,
    JUMP_IF = 61,
    TRY = 62,
    TRY_END = 63,
    CALL = 64,
    SPAWN = 65,
    RAISE = 66,
    RETURN = 67,
    YIELD = 68,
    RAISE_IF_NOT_STOP = 69,
    OP_ADD = 80,
    OP_SUB = 81,
    OP_MUL = 82,
    OP_DIV = 83,
    OP_MOD = 84,
    OP_AND = 85,
    OP_OR = 86,
    OP_XOR = 87,
    OP_NEG = 88,
    OP_NOT = 89,
    OP_INV = 90,
    OP_EQ = 91,
    OP_NE = 92,
    OP_GT = 93,
    OP_LT = 94,
    OP_GE = 95,
    OP_LE = 96,
    OP_SHL = 97,
    OP_SHR = 98,
}

fn interpreter(code: &BytesObject, context: &DictObject, args: &TupleObject, gil: &GCellOwner) {
    let mut locals = context.clone();
    let mut pc = 0;
    loop {}
}

fn interpreter_inner(
    gil: &mut GCellOwner,
    code: &mut Cursor<&[u8]>,
    locals: &mut DictObject,
    stack: &mut Vec<Object>,
) -> Result<StepResult> {
    let op = next_opcode(code)?;
    match op {
        Opcode::ERROR => {
            Err(ExceptionObject::runtime_error("Invalid opcode")?)
        }
        Opcode::ST_SWAP => {
            if let [.., first, second] = stack.as_mut_slice() {
                mem::swap(first, second);
                Ok(StepResult::Normal)
            } else {
                Err(ExceptionObject::runtime_error("Stack underflow")?)
            }
        }
        Opcode::ST_POP => {
            fallible_pop(stack)?;
            Ok(StepResult::Normal)
        }
        Opcode::ST_DUP => {
            if let Some(obj) = stack.last().cloned() {
                fallible_push(stack, obj)?;
                Ok(StepResult::Normal)
            } else {
                Err(ExceptionObject::runtime_error("Stack underflow")?)
            }
        }
        Opcode::ST_DUP2 => {
            if let Some(st2) = stack.get(stack.len().wrapping_sub(2)).cloned() {
                fallible_push(stack, st2)?;
                Ok(StepResult::Normal)
            } else {
                Err(ExceptionObject::runtime_error("Stack underflow")?)
            }
        }
        Opcode::LIT_BYTES => {
            let val = next_bytes(code)?;
            fallible_push(stack, val.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::LIT_INT => {
            let val = IntObject::new(next_signed(code)?)?;
            fallible_push(stack, val.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::LIT_FLOAT => {
            let val = FloatObject::new(next_float(code)?)?;
            fallible_push(stack, val.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::LIT_SLICE => {
            let val2 = fallible_pop(stack)?;
            let val1 = fallible_pop(stack)?;
            todo!()
        }
        Opcode::LIT_NONE => {
            fallible_push(stack, G_NONE.clone().into())?;
            Ok(StepResult::Normal)
        }
        Opcode::LIT_TRUE => {
            fallible_push(stack, G_TRUE.clone().into())?;
            Ok(StepResult::Normal)
        }
        Opcode::LIT_FALSE => {
            fallible_push(stack, G_FALSE.clone().into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_0 => {
            fallible_push(stack, TupleObject::new0()?.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_1 => {
            let val = fallible_pop(stack)?;
            fallible_push(stack, TupleObject::new1(val)?.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_2 => {
            let val2 = fallible_pop(stack)?;
            let val1 = fallible_pop(stack)?;
            fallible_push(stack, TupleObject::new2(val1, val2)?.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_3 => {
            let val3 = fallible_pop(stack)?;
            let val2 = fallible_pop(stack)?;
            let val1 = fallible_pop(stack)?;
            fallible_push(stack, TupleObject::new3(val1, val2, val3)?.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_4 => {
            let val4 = fallible_pop(stack)?;
            let val3 = fallible_pop(stack)?;
            let val2 = fallible_pop(stack)?;
            let val1 = fallible_pop(stack)?;
            fallible_push(stack, TupleObject::new4(val1, val2, val3, val4)?.into())?;
            Ok(StepResult::Normal)
        }
        Opcode::TUPLE_N => {
            let len = next_unsigned(code)? as usize;
            if len > stack.len() {
                Err(ExceptionObject::runtime_error("Stack underflow")?)
            } else {
                let mut vals = vec![];
                vec_reserve(&mut vals, len)?;
                vals.extend_from_slice(&stack[stack.len() - len..]);
                stack.truncate(stack.len() - len);
                fallible_push(stack, TupleObject::new(vals)?.into())?;
                Ok(StepResult::Normal)
            }
        }
        Opcode::CLOSURE => {
            todo!()
        }
        Opcode::CLOSURE_BIND => {
            todo!()
        }
        Opcode::EMPTY_DICT => {
            todo!()
        }
        Opcode::CLASS => {
            todo!()
        }
        Opcode::GET_ATTR => {
            let name_obj = fallible_pop(stack)?;
            let obj = fallible_pop(stack)?;
            let result = obj.get_attr(gil, name_obj.as_ref())?;
            fallible_push(stack, result)?;
            Ok(StepResult::Normal)
        }
        Opcode::SET_ATTR => {
            let value = fallible_pop(stack)?;
            let name_obj = fallible_pop(stack)?;
            let obj = fallible_pop(stack)?;
            obj.set_attr(gil, name_obj.as_ref(), value)?;
            Ok(StepResult::Normal)
        }
        Opcode::DEL_ATTR => {
            todo!()
        }
        Opcode::GET_ITEM => {
            todo!()
        }
        Opcode::SET_ITEM => {
            todo!()
        }
        Opcode::DEL_ITEM => {
            todo!()
        }
        Opcode::GET_LOCAL => {
            todo!()
        }
        Opcode::SET_LOCAL => {
            todo!()
        }
        Opcode::DEL_LOCAL => {
            todo!()
        }
        Opcode::LOAD_ARGS => {
            todo!()
        }
        Opcode::JUMP => {
            todo!()
        }
        Opcode::JUMP_IF => {
            todo!()
        }
        Opcode::TRY => {
            todo!()
        }
        Opcode::TRY_END => {
            todo!()
        }
        Opcode::CALL => {
            todo!()
        }
        Opcode::SPAWN => {
            todo!()
        }
        Opcode::RAISE => {
            todo!()
        }
        Opcode::RETURN => {
            todo!()
        }
        Opcode::YIELD => {
            todo!()
        }
        Opcode::RAISE_IF_NOT_STOP => {
            todo!()
        }
        Opcode::OP_ADD => {
            todo!()
        }
        Opcode::OP_SUB => {
            todo!()
        }
        Opcode::OP_MUL => {
            todo!()
        }
        Opcode::OP_DIV => {
            todo!()
        }
        Opcode::OP_MOD => {
            todo!()
        }
        Opcode::OP_AND => {
            todo!()
        }
        Opcode::OP_OR => {
            todo!()
        }
        Opcode::OP_XOR => {
            todo!()
        }
        Opcode::OP_NEG => {
            todo!()
        }
        Opcode::OP_NOT => {
            todo!()
        }
        Opcode::OP_INV => {
            todo!()
        }
        Opcode::OP_EQ => {
            todo!()
        }
        Opcode::OP_NE => {
            todo!()
        }
        Opcode::OP_GT => {
            todo!()
        }
        Opcode::OP_LT => {
            todo!()
        }
        Opcode::OP_GE => {
            todo!()
        }
        Opcode::OP_LE => {
            todo!()
        }
        Opcode::OP_SHL => {
            todo!()
        }
        Opcode::OP_SHR => {
            todo!()
        }
    }
}

fn next_opcode(code: &mut Cursor<&[u8]>) -> Result<Opcode> {
    if code.is_empty() {
        Err(ExceptionObject::runtime_error("End of bytecode")?)
    } else {
        code.read_u8()
            .unwrap()
            .try_into()
            .or_else(|_| Err(ExceptionObject::runtime_error("Invalid opcode")?))
    }
}

fn next_float(code: &mut Cursor<&[u8]>) -> Result<f64> {
    if let Ok(f) = code.read_f64::<byteorder::LE>() {
        Ok(f)
    } else {
        Err(ExceptionObject::runtime_error("End of bytecode")?)
    }
}

fn next_unsigned(code: &mut Cursor<&[u8]>) -> Result<u64> {
    match leb128::read::unsigned(code) {
        Ok(len) => Ok(len),
        Err(leb128::read::Error::Overflow) => Err(ExceptionObject::overflow_error(
            "Literal integer too large",
        )?),
        Err(leb128::read::Error::IoError(_)) => {
            Err(ExceptionObject::runtime_error("End of bytecode")?)
        }
    }
}

fn next_signed(code: &mut Cursor<&[u8]>) -> Result<i64> {
    match leb128::read::signed(code) {
        Ok(len) => Ok(len),
        Err(leb128::read::Error::Overflow) => Err(ExceptionObject::overflow_error(
            "Literal integer too large",
        )?),
        Err(leb128::read::Error::IoError(_)) => {
            Err(ExceptionObject::runtime_error("End of bytecode")?)
        }
    }
}

fn next_bytes(code: &mut Cursor<&[u8]>) -> Result<BytesObject> {
    let len = next_unsigned(code)?;
    let mut buf = vec![0; len as usize];
    match code.read_exact(&mut buf) {
        Ok(()) => Ok(BytesObject {
            ty: G_BYTES.clone(),
            value: buf,
        }
        .into()),
        Err(_) => Err(ExceptionObject::runtime_error("End of bytecode")?),
    }
}
