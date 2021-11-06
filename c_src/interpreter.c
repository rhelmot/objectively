#include <stdio.h>

#include "gc.h"
#include "object.h"
#include "builtins.h"
#include "errors.h"
#include "thread.h"

typedef enum Opcode {
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
	RAISE_IF_NOT_STOP = 69, // I swear to god I didn't give the funniest opcode the funniest number on purpose
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
} Opcode;

#define ENSURE_BYTES(n) ((size_t)(*pointer - bytes_data(bytecode) + (n)) <= bytecode->len)

Opcode next_opcode(BytesObject *bytecode, const char **pointer) {
	if (!ENSURE_BYTES(1)) {
		return ERROR;
	}
	Opcode result = **pointer;
	(*pointer)++;
	return result;
}

bool next_num_unsigned(BytesObject *bytecode, const char **pointer, uint64_t *out) {
	// https://en.wikipedia.org/wiki/LEB128
	uint64_t result = 0;
	uint64_t shift = 0;
	while (true) {
		if (!ENSURE_BYTES(1)) {
			return false;
		}
		unsigned char next = **pointer;
		(*pointer)++;
		result |= (uint64_t)(next & 0x7f) << shift;
		shift += 7;
		if ((next & 0x80) == 0) {
			*out = result;
			return true;
		}
	}
}

bool next_num_signed(BytesObject *bytecode, const char **pointer, int64_t *out) {
	int64_t result = 0;
	uint64_t shift = 0;
	while (true) {
		if (!ENSURE_BYTES(1)) {
			return false;
		}
		unsigned char next = **pointer;
		(*pointer)++;
		result |= (uint64_t)(next & 0x7f) << shift;
		shift += 7;
		if ((next & 0x80) == 0) {
			if (shift < 64 && (next & 0x40)) {
				result |= ~0 << shift;
			}
			*out = result;
			return true;
		}
	}
}

bool next_float(BytesObject *bytecode, const char **pointer, double *out) {
	if (!ENSURE_BYTES(sizeof(double))) {
		return false;
	}
	*out = *(double*)*pointer;
	*pointer += sizeof(double);
	return true;
}

bool next_offset(BytesObject *bytecode, const char **pointer, size_t *out) {
	if (!ENSURE_BYTES(sizeof(uint32_t))) {
		return false;
	}
	*out = *(uint32_t*)*pointer;
	*pointer += sizeof(uint32_t);
	return true;
}

BytesUnownedObject *next_bytes(BytesObject *bytecode, const char **pointer) {
	uint64_t _size;
	if (!next_num_unsigned(bytecode, pointer, &_size)) {
		return NULL;
	}
	size_t size = _size;
	if (!ENSURE_BYTES(size)) {
		return NULL;
	}
	BytesUnownedObject *result = bytes_unowned_raw(*pointer, size, (Object*)bytecode);
	*pointer += size;
	return result;
}

Object *interpreter(ClosureObject *closure, TupleObject *args) {
	DictObject *locals = dict_dup_inner(closure->context);
	if (locals == NULL) {
		return NULL;
	}
	gc_root((Object*)locals);

	ListObject *stack = list_raw(NULL, 0);
	if (stack == NULL) {
		gc_unroot((Object*)locals);
		return NULL;
	}
	gc_root((Object*)stack);

	ListObject *temproot = list_raw(NULL, 0);
	if (temproot == NULL) {
		gc_unroot((Object*)stack);
		gc_unroot((Object*)locals);
		return NULL;
	}
	gc_root((Object*)temproot);

	ListObject *trystack = list_raw(NULL, 0);
	if (trystack == NULL) {
		gc_unroot((Object*)stack);
		gc_unroot((Object*)locals);
		gc_unroot((Object*)temproot);
		return NULL;
	}
	gc_root((Object*)trystack);

	const char *pointer = bytes_data(closure->bytecode);
	Object *result = NULL;

	while (true) {
		while (temproot->len) {
			if (!list_pop_back_inner(temproot)) {
				puts("Fatal error: could not pop temproot");
				exit(1);
			}
		}
		gc_probe();
		if (!gil_probe()) {
			goto ERROR;
		}
		if (pointer == &bytes_data(closure->bytecode)[closure->bytecode->len]) {
			result = (Object*)&g_none;
			break;
		}
		Opcode opcode = next_opcode(closure->bytecode, &pointer);
		switch (opcode) {

#define CHECK(_val) ({ __typeof__(_val) _evaluated = (_val); if (!_evaluated) { break; } _evaluated; })
#define POP() ({ Object *_popped = list_pop_back_inner(stack); if (_popped == NULL) { puts("Fatal error: stack underflow"); exit(1); } _popped; })
#define PUSH(_pushed) ({ if (!list_push_back_inner(stack, (Object*)_pushed)) { break; } })
#define TEMPROOT(_rooted) ({ if (!list_push_back_inner(temproot, (Object*)_rooted)) { break ; } })
#define NEXT_NUM_UNSIGNED() ({ uint64_t _lit; if (!next_num_unsigned(closure->bytecode, &pointer, &_lit)) { error = exc_msg(&g_RuntimeError, "out of bounds"); break; } _lit; })
#define NEXT_NUM_SIGNED() ({ int64_t _lit; if (!next_num_signed(closure->bytecode, &pointer, &_lit)) { error = exc_msg(&g_RuntimeError, "out of bounds"); break; } _lit; })
#define NEXT_FLOAT() ({ double _lit; if (!next_float(closure->bytecode, &pointer, &_lit)) { error = exc_msg(&g_RuntimeError, "out of bounds"); break; } _lit; })
#define NEXT_OFFSET() ({ size_t _lit; if (!next_offset(closure->bytecode, &pointer, &_lit)) { error = exc_msg(&g_RuntimeError, "out of bounds"); break; } _lit; })
#define NEXT_BYTES() ({ BytesUnownedObject *_lit = next_bytes(closure->bytecode, &pointer); if (_lit == NULL) { error = exc_msg(&g_RuntimeError, "out of bounds"); break; } _lit; })
#define TEMP_ARGS0() ({ TupleObject *_tmp = tuple_raw(NULL, 0); if (_tmp == NULL) { break; } TEMPROOT(_tmp); _tmp; })
#define TEMP_ARGS1(arg1) ({ TupleObject *_tmp = tuple_raw((Object*[]){arg1}, 1); if (_tmp == NULL) { break; } TEMPROOT(_tmp); _tmp; })
#define TEMP_ARGS2(arg1, arg2) ({ TupleObject *_tmp = tuple_raw((Object*[]){arg1, arg2}, 2); if (_tmp == NULL) { break; } TEMPROOT(_tmp); _tmp; })
#define TEMP_ARGS3(arg1, arg2, arg3) ({ TupleObject *_tmp = tuple_raw((Object*[]){arg1, arg2, arg3}, 3); if (_tmp == NULL) { break; } TEMPROOT(_tmp); _tmp; })
#define BINOP(methodname) { Object *arg2 = POP(); Object *arg1 = POP(); Object *method = CHECK(get_attr_inner(arg1, "__" #methodname "__")); PUSH(CHECK(call(method, TEMP_ARGS1(arg2)))); }
#define UNOP(methodname) { Object *arg1 = POP(); Object *method = CHECK(get_attr_inner(arg1, "__" #methodname "__")); PUSH(CHECK(call(method, TEMP_ARGS0()))); }

			case ST_SWAP: {
				Object *a1 = POP();
				Object *a2 = POP();
				PUSH(a1);
				PUSH(a2);
				continue;
			}
			case ST_POP: {
				POP();
				continue;
			}
			case ST_DUP: {
				Object *a1 = POP();
				PUSH(a1);
				PUSH(a1);
				continue;
			}
			case ST_DUP2: {
				Object *a2 = POP();
				Object *a1 = POP();
				PUSH(a1);
				PUSH(a2);
				PUSH(a1);
				PUSH(a2);
				continue;
			}
			case LIT_BYTES: {
				PUSH(NEXT_BYTES());
				continue;
			}
			case LIT_INT: {
				PUSH(int_raw(NEXT_NUM_SIGNED()));
				continue;
			}
			case LIT_FLOAT: {
				PUSH(float_raw(NEXT_FLOAT()));
				continue;
			}
			case LIT_SLICE: {
				Object *end = POP();
				Object *start = POP();
				PUSH(slice_raw(start, end));
				continue;
			}
			case LIT_NONE: {
				PUSH(&g_none);
				continue;
			}
			case LIT_TRUE: {
				PUSH(&g_true);
				continue;
			}
			case LIT_FALSE: {
				PUSH(&g_false);
				continue;
			}
			case TUPLE_0:
			case TUPLE_1:
			case TUPLE_2:
			case TUPLE_3:
			case TUPLE_4:
			case TUPLE_N: {
				size_t count;
				if (opcode == TUPLE_N) {
					count = NEXT_NUM_UNSIGNED();
				} else {
					count = opcode - TUPLE_0;
				}
				if (stack->len < count) {
					error = exc_msg(&g_RuntimeError, "stack underflow");
					break;
				}
				TupleObject *result = tuple_raw(&stack->data[stack->len - count], count);
				if (result == NULL) {
					break;
				}
				for (size_t i = 0; i < count; i++) {
					POP();
				}
				PUSH(result);
				continue;
			}
			case CLOSURE: {
				Object *code = POP();
				if (code->type != &g_bytes) {
					error = exc_msg(&g_TypeError, "Expected bytes");
					break;
				}
				PUSH(CHECK(closure_raw((BytesObject*)code, locals)));
				continue;
			}
			case CLOSURE_BIND: {
				Object *code = POP();
				uint64_t num_idents = NEXT_NUM_UNSIGNED();
				if (code->type != &g_bytes) {
					error = exc_msg(&g_TypeError, "Expected bytes");
					break;
				}

				DictObject *new_context = CHECK(dicto_raw());
				TEMPROOT(new_context);
				for (uint64_t i = 0; i < num_idents; i++) {
					BytesUnownedObject *name = NEXT_BYTES();
					Object *value = CHECK(dict_getitem(TEMP_ARGS2((Object*)locals, (Object*)name)));
					CHECK(dict_setitem(TEMP_ARGS3((Object*)new_context, (Object*)name, value)));
				}

				PUSH(CHECK(closure_raw((BytesObject*)code, new_context)));
				continue;
			}
			case EMPTY_DICT: {
				PUSH(CHECK(dicto_raw()));
				continue;
			}
			case CLASS: {
				Object *dict = POP();
				Object *base = POP();
				PUSH(CHECK(type_constructor((Object*)&g_type, TEMP_ARGS2(base, dict))));
				continue;
			}
			case GET_ATTR: {
				Object *name = POP();
				Object *obj = POP();
				PUSH(CHECK(get_attr(obj, name)));
				continue;
			}
			case SET_ATTR: {
				Object *value = POP();
				Object *name = POP();
				Object *obj = POP();
				CHECK(set_attr(obj, name, value));
				continue;
			}
			case DEL_ATTR: {
				Object *name = POP();
				Object *obj = POP();
				CHECK(!del_attr(obj, name));
				continue;
			}
			case GET_ITEM: {
				Object *key = POP();
				Object *obj = POP();
				TEMPROOT(obj);
				Object *getattr = CHECK(get_attr_inner(obj, "__getitem__"));
				PUSH(CHECK(call(getattr, TEMP_ARGS1(key))));
				continue;
			}
			case SET_ITEM: {
				Object *val = POP();
				Object *key = POP();
				Object *obj = POP();
				TEMPROOT(obj);
				Object *setattr = CHECK(get_attr_inner(obj, "__setitem__"));
				CHECK(call(setattr, TEMP_ARGS2(key, val)));
				continue;
			}
			case DEL_ITEM: {
				Object *key = POP();
				Object *obj = POP();
				TEMPROOT(obj);
				Object *delattr = CHECK(get_attr_inner(obj, "__delitem__"));
				CHECK(call(delattr, TEMP_ARGS1(key)));
				continue;
			}
			case GET_LOCAL: {
				Object *name = POP();
				PUSH(CHECK(dict_getitem(TEMP_ARGS2((Object*)locals, name))));
				continue;
			}
			case SET_LOCAL: {
				Object *val = POP();
				Object *name = POP();
				CHECK(dict_setitem(TEMP_ARGS3((Object*)locals, name, val)));
				continue;
			}
			case DEL_LOCAL: {
				Object *name = POP();
				CHECK(dict_popitem(TEMP_ARGS2((Object*)locals, name)));
				continue;
			}
			case LOAD_ARGS: {
				PUSH(args);
				continue;
			}
			case JUMP: {
				pointer = &bytes_data(closure->bytecode)[NEXT_OFFSET()];
				continue;
			}
			case JUMP_IF: {
				size_t off = NEXT_OFFSET();
				Object *cond = POP();
				TEMPROOT(cond);
				Object *boolfunc = CHECK(get_attr_inner(cond, "__bool__"));
				Object *evaluated_cond = CHECK(call(boolfunc, TEMP_ARGS0()));
				if (evaluated_cond->type != &g_bool) {
					error = exc_msg(&g_TypeError, "__bool__ did not return bool");
					break;
				}
				if (evaluated_cond == (Object*)&g_true) {
					pointer = &bytes_data(closure->bytecode)[off];
				}
				continue;
			}
			case TRY: {
				size_t catch_target = NEXT_OFFSET();
				CHECK(list_push_back_inner(trystack, (Object*)int_raw((int64_t)catch_target)));
				continue;
			}
			case TRY_END: {
				CHECK(list_pop_back_inner(trystack));
				continue;
			}
			case CALL: {
				Object *args = POP();
				Object *target = POP();
				if (args->type != &g_tuple) {
					error = exc_msg(&g_TypeError, "Expected tuple");
					break;
				}
				TEMPROOT(args);
				TEMPROOT(target);
				PUSH(CHECK(call(target, (TupleObject*)args)));
				continue;
			}
			case SPAWN: {
				Object *args = POP();
				Object *target = POP();
				if (args->type != &g_tuple) {
					error = exc_msg(&g_TypeError, "Expected tuple");
					break;
				}
				TEMPROOT(args);
				TEMPROOT(target);
				PUSH(CHECK(thread_raw(target, (TupleObject*)args, &g_thread)));
				continue;
			}
			case RAISE: {
				error = POP();
				break;
			}
			case RETURN: {
				result = POP();
				goto EXIT;
			}
			case YIELD: {
				Object *val = POP();
				CHECK(thread_yield(val));
				continue;
			}
			case RAISE_IF_NOT_STOP: {
				Object *e = POP();
				if (!isinstance_inner(e, &g_StopIteration)) {
					error = e;
					break;
				}
				continue;
			}
			case OP_ADD: {
				BINOP(add);
				continue;
			}
			case OP_SUB: {
				BINOP(sub);
				continue;
			}
			case OP_MUL: {
				BINOP(mul);
				continue;
			}
			case OP_DIV: {
				BINOP(div);
				continue;
			}
			case OP_MOD: {
				BINOP(mod);
				continue;
			}
			case OP_AND: {
				BINOP(and);
				continue;
			}
			case OP_OR: {
				BINOP(or);
				continue;
			}
			case OP_XOR: {
				BINOP(xor);
				continue;
			}
			case OP_NEG: {
				UNOP(neg);
				continue;
			}
			case OP_NOT: {
				UNOP(not);
				continue;
			}
			case OP_INV: {
				UNOP(inv);
				continue;
			}
			case OP_EQ: {
				BINOP(eq);
				continue;
			}
			case OP_NE: {
				BINOP(ne);
				continue;
			}
			case OP_GT: {
				BINOP(gt);
				continue;
			}
			case OP_LT: {
				BINOP(lt);
				continue;
			}
			case OP_GE: {
				BINOP(ge);
				continue;
			}
			case OP_LE: {
				BINOP(le);
				continue;
			}
			case OP_SHL: {
				BINOP(shl);
				continue;
			}
			case OP_SHR: {
				BINOP(shr);
				continue;
			}
			default: {
				error = exc_msg(&g_RuntimeError, "Bad opcode");
				break;
			}
		}

		Object *local_error, *_catch_target;
ERROR:
		local_error = error;
		_catch_target = list_pop_back_inner(trystack);
		if (_catch_target == NULL) {
			error = local_error;
			goto EXIT;
		}
		if (_catch_target->type != (TypeObject*)&g_int) {
			puts("Fatal error: catch target was not int");
			exit(1);
		}
		size_t catch_target = ((IntObject*)_catch_target)->value;
		pointer = &bytes_data(closure->bytecode)[catch_target];

		while (stack->len) {
			POP(); // ok to use this outside the switch
		}
		if (!list_push_back_inner(stack, error)) {
			error = (Object*)&MemoryError_inst;
			// disard the entire try stack and just bail out
			goto EXIT;
		}
		continue;
	}

EXIT:
	gc_unroot((Object*)stack);
	gc_unroot((Object*)locals);
	gc_unroot((Object*)temproot);
	gc_unroot((Object*)trystack);
	return result;
}
