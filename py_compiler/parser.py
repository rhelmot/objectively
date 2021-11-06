import struct
from lexer import tokens
import leb128

start = 'statement_list'

OPCODES = dict(
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
)

precedence = (
        ('left', 'BITOR'),
        ('left', 'XOR'),
        ('left', 'BITAND'),
        ('left', 'EQ', 'NE'), 
        ('left', 'GT', 'LT', 'GE', 'LE'),
        ('left', 'SHL', 'SHR'),
        ('left', 'PLUS', 'MINUS'),
        ('left', 'TIMES', 'DIV', 'MOD'),
)

l_CONTINUE = object()
l_BREAK = object()

class Linkable:
    def __init__(self, *bytecode, symbols=None, relocations=None):
        if symbols is None:
            symbols = {}
        if relocations is None:
            relocations = {}
        real_bytecode = bytearray()
        for b in bytecode:
            if type(b) is int:
                real_bytecode.append(b)
            elif type(b) in (bytes, bytearray):
                real_bytecode.extend(b)
            else:
                raise TypeError(type(b))
        self.bytecode = [real_bytecode]
        self.symbols = symbols
        self.relocations = relocations

    def __len__(self):
        return sum(len(b) for b in self.bytecode)

    def relocated_symbols(self, offset):
        return {symbol: addr + offset for symbol, addr in self.symbols.items()}

    def relocated_relocations(self, offset):
        return {addr + offset: symbol for addr, symbol in self.relocations.items()}

    def append(self, other):
        offset = len(self)
        self.bytecode.extend(other.bytecode)
        if set(self.symbols) & set(other.symbols):
            raise Exception("Duplicate label definition")
        self.symbols.update(other.relocated_symbols(offset))
        self.relocations.update(other.relocated_relocations(offset))
        return self

    def link(self):
        result = bytearray()
        for b in self.bytecode:
            result.extend(b)
        for addr, symbol in self.relocations.items():
            target = self.symbols[symbol]
            struct.pack_into('I', result, addr, target)
        return result

    def get(self):
        return self

class LValue:
    def __init__(self, bytecode, options):
        self.bytecode = bytecode
        self.options = options

    def get(self):
        return self.bytecode.append(Linkable(self.options['get']))

    def set(self, value):
        return self.bytecode.append(value.get()).append(Linkable(self.options['set']))

    def set_update(self, value, op):
        dup_mapped = {
                1: 'ST_DUP',
                2: 'ST_DUP2',
        }[self.options['args']]
        return self.bytecode.append(Linkable(bytes([OPCODES[dup_mapped]]))).append(Linkable(self.options['get'])).append(value.get()).append(Linkable(bytes([OPCODES[op]]))).append(Linkable(self.options['set']))

    def del_(self, value):
        return self.bytecode.append(Linkable(self.options['del']))

OPTIONS_IDENT = {
        'get': bytes([OPCODES['GET_LOCAL']]),
        'set': bytes([OPCODES['SET_LOCAL']]),
        'del': bytes([OPCODES['DEL_LOCAL']]),
        'args': 1,
}
OPTIONS_ITEM = {
        'get': bytes([OPCODES['GET_ITEM']]),
        'set': bytes([OPCODES['SET_ITEM']]),
        'del': bytes([OPCODES['DEL_ITEM']]),
        'args': 2,
}
OPTIONS_ATTR = {
        'get': bytes([OPCODES['GET_ATTR']]),
        'set': bytes([OPCODES['SET_ATTR']]),
        'del': bytes([OPCODES['DEL_ATTR']]),
        'args': 2,
}

def encode_bytes(bytestring):
    if type(bytestring) is str:
        bytestring = bytestring.encode()
    return leb128.u.encode(len(bytestring)) + bytestring

def p_expression_0_int(p):
    "expression_0 : LIT_INT"
    p[0] = Linkable(bytes([OPCODES['LIT_INT']]) + leb128.i.encode(p[1]))

def p_expression_0_float(p):
    "expression_0 : LIT_FLOAT"
    p[0] = Linkable(bytes([OPCODES['LIT_FLOAT']]) + struct.pack('d', p[1]))

def p_expression_0_bytes(p):
    "expression_0 : LIT_BYTES"
    p[0] = Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(p[1]))

def p_expression_0_ident(p):
    "expression_0 : IDENT"
    p[0] = LValue(Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(p[1])), OPTIONS_IDENT)

def p_expression_0_parens(p):
    "expression_0 : LPAREN expression_4 RPAREN"
    p[0] = p[2]

def p_expression_0_tuple(p):
    "expression_0 : LBRACKET expression_list RBRACKET"
    p[0] = Linkable(b'')
    for elem in p[2]:
        p[0].append(elem.get())
    p[0].append(Linkable(bytes([OPCODES['TUPLE_N']]) + leb128.u.encode(len(p[2]))))

def p_expression_0_fn(p):
    "expression_0 : FN LPAREN ident_list RPAREN optional_scope LBRACE statement_list RBRACE"
    function_text = Linkable(b'')
    for i, arg in enumerate(p[3]):
        function_text.append(Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(arg) + bytes([OPCODES['LOAD_ARGS'], OPCODES['LIT_INT']]) + leb128.u.encode(i) + bytes([OPCODES['GET_ITEM'], OPCODES['SET_LOCAL']])))
    function_text = function_text.append(p[7]).link()
    p[0] = Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(function_text))
    if p[5] is None:
        p[0].append(Linkable(bytes([OPCODES['CLOSURE']])))
    else:
        p[0].append(Linkable(bytes([OPCODES['CLOSURE_BIND']]) + leb128.u.encode(len(p[5]))))
        for bind in p[5]:
            p[0].append(Linkable(encode_bytes(bind)))

def p_expression_0_class(p):
    "expression_0 : CLASS LPAREN IDENT RPAREN LBRACE class_members RBRACE"
    p[0] = Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(p[3]) + bytes([OPCODES['GET_LOCAL'], OPCODES['EMPTY_DICT']]))
    for name, expr in p[6]:
        p[0].append(Linkable(bytes([OPCODES['ST_DUP'], OPCODES['LIT_BYTES']]) + encode_bytes(name))).append(expr).append(Linkable(bytes([OPCODES['SET_ITEM']])))
    p[0].append(Linkable(bytes([OPCODES['CLASS']])))

def p_class_members(p):
    """class_members : class_members_inner
                     | """ # empty
    if len(p) == 1:
        p[0] = []
    else:
        p[0] = p[1]

def p_class_members_inner(p):
    """class_members_inner : class_member
                           | class_members_inner class_member"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[1].append(p[2])

def p_class_member(p):
    "class_member : IDENT ASSIGN expression_4 SEMICOLON"
    p[0] = (p[1], p[3])

def p_optional_scope(p):
    """optional_scope : LBRACKET ident_list RBRACKET
                      | """ # empty
    if len(p) == 1:
        p[0] = None
    else:
        p[0] = p[2]

def p_expression_1_escape(p):
    "expression_1 : expression_0"
    p[0] = p[1]

def p_expression_1_attr(p):
    "expression_1 : expression_1 DOT IDENT"
    p[0] = LValue(p[1].get().append(Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(p[3]))), OPTIONS_ATTR)

def p_expression_1_item(p):
    "expression_1 : expression_1 LBRACKET expression_4 RBRACKET"
    p[0] = LValue(p[1].get().append(p[3].get()), OPTIONS_ITEM)

def p_expression_1_slice(p):
    "expression_1 : expression_1 LBRACKET expression_optional COLON expression_optional RBRACKET"
    slice_expr = Linkable(bytes())
    slice_expr.append(p[3].get() if p[3] is not None else Linkable(bytes([OPCODES['LIT_NONE']])))
    slice_expr.append(p[5].get() if p[5] is not None else Linkable(bytes([OPCODES['LIT_NONE']])))
    slice_expr.append(Linkable(bytes([OPCODES['LIT_SLICE']])))
    p[0] = LValue(p[1].get().append(slice_expr), OPTIONS_ITEM)

def p_expression_1_call(p):
    "expression_1 : expression_1 LPAREN expression_list RPAREN"
    p[0] = p[1].get()
    for expr in p[3]:
        p[0].append(expr.get())
    p[0].append(Linkable(bytes([OPCODES['TUPLE_N']]) + leb128.u.encode(len(p[3])) + bytes([OPCODES['CALL']])))

def p_expression_1_spawn(p):
    "expression_1 : SPAWN expression_1 LPAREN expression_list RPAREN"
    p[0] = p[2].get()
    for expr in p[4]:
        p[0].append(expr.get())
    p[0].append(Linkable(bytes([OPCODES['TUPLE_N']]) + leb128.u.encode(len(p[4])) + bytes([OPCODES['SPAWN']])))

def p_expression_2_escape(p):
    "expression_2 : expression_1"
    p[0] = p[1]

def p_expression_2_unop(p):
    """
    expression_2 : MINUS expression_2
                 | NOT expression_2
                 | INV expression_2
                 | PLUS expression_2
    """
    op_mapped = {
            '-': 'OP_NEG',
            '!': 'OP_NOT',
            '~': 'OP_INV',
            '+': None
    }[p[1]]
    
    p[0] = p[2].get()
    if op_mapped is not None:
        p[0].append(Linkable(bytes([OPCODES[op_mapped]])))

def p_expression_3_escape(p):
    "expression_3 : expression_2"
    p[0] = p[1]

def p_expression_3_binop(p):
    """
    expression_3 : expression_3 PLUS expression_3
                 | expression_3 MINUS expression_3
                 | expression_3 TIMES expression_3
                 | expression_3 DIV expression_3
                 | expression_3 MOD expression_3
                 | expression_3 BITAND expression_3
                 | expression_3 BITOR expression_3
                 | expression_3 XOR expression_3
                 | expression_3 EQ expression_3
                 | expression_3 NE expression_3
                 | expression_3 GT expression_3
                 | expression_3 LT expression_3
                 | expression_3 GE expression_3
                 | expression_3 LE expression_3
                 | expression_3 SHL expression_3
                 | expression_3 SHR expression_3
    """
    op_mapped = {
        '+': 'OP_ADD',
        '-': 'OP_SUB',
        '*': 'OP_MUL',
        '/': 'OP_DIV',
        '%': 'OP_MOD',
        '&': 'OP_AND',
        '|': 'OP_OR',
        '^': 'OP_XOR',
        '==': 'OP_EQ',
        '!=': 'OP_NE',
        '>': 'OP_GT',
        '<': 'OP_LT',
        '>=': 'OP_GE',
        '<=': 'OP_LE',
        '<<': 'OP_SHL',
        '>>': 'OP_SHR',
    }[p[2]]

    p[0] = p[1].get().append(p[3].get()).append(Linkable(bytes([OPCODES[op_mapped]])))

def p_expression_4_escape(p):
    "expression_4 : expression_3"
    p[0] = p[1]

def p_expression_4_and(p):
    "expression_4 : expression_4 LOGAND expression_4"
    p[0] = p[1].get()
    # <expr_1>
    # dup
    # neg
    # jmp_if <end>
    # pop
    # <expr_2>
    end = object()
    code = Linkable(bytes([OPCODES['OP_DUP'], OPCODES['OP_NOT'], OPCODES['JUMP_IF'], 0,0,0,0, OPCODES['ST_POP']]), relocations={3: end})
    p[0].append(code).append(p[3].get())
    p[0].symbols[end] = len(p[0])

def p_expression_4_or(p):
    "expression_4 : expression_4 LOGOR expression_4"
    p[0] = p[1].get()
    end = object()
    code = Linkable(bytes([OPCODES['OP_DUP'], OPCODES['JUMP_IF'], 0,0,0,0, OPCODES['ST_POP']]), relocations={2: end})
    p[0].append(code).append(p[3].get())
    p[0].symbols[end] = len(p[0])

def p_expression_optional(p):
    """expression_optional : expression_4
                           | """ # empty
    if len(p) == 1:
        p[0] = None
    else:
        p[0] = p[1]

def p_ident_list(p):
    """ident_list : ident_list_inner
                  | """ # empty
    if len(p) == 1:
        p[0] = []
    else:
        p[0] = p[1]

def p_ident_list_inner(p):
    """ident_list_inner : IDENT
                        | ident_list_inner COMMA IDENT"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[0].append(p[3])

def p_expression_list(p):
    """expression_list : expression_list_inner
                       | """ # empty
    if len(p) == 1:
        p[0] = []
    else:
        p[0] = p[1]

def p_expression_list_inner(p):
    """expression_list_inner : expression_4
                             | expression_list_inner COMMA expression_4"""
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[0].append(p[3])

def p_statement_list(p):
    """statement_list : statement_list_inner
                      | """ # empty
    if len(p) == 1:
        p[0] = Linkable(b'')
    else:
        p[0] = p[1]

def p_statement_list_inner(p):
    """statement_list_inner : statement
                            | statement_list_inner statement"""
    if len(p) == 2:
        p[0] = p[1]
    else:
        p[0] = p[1]
        p[0].append(p[2])

def p_statement_expression(p):
    "statement : expression_4 SEMICOLON"
    p[0] = p[1]
    p[0].append(Linkable(bytes([OPCODES['ST_POP']])))

def p_statement_assignment(p):
    "statement : expression_4 assign_op expression_4 SEMICOLON"
    if type(p[1]) is not LValue:
        print("Error: left hand side of assignment must be an lvalue")
        raise SyntaxError
    if p[2] == '=':
        p[0] = p[1].set(p[3])
    else:
        op_mapped = {
                '+=': 'OP_ADD',
                '-=': 'OP_SUB',
                '*=': 'OP_MUL',
                '/=': 'OP_DIV',
                '%=': 'OP_MOD',
                '&=': 'OP_AND',
                '|=': 'OP_OR',
                '^=': 'OP_XOR',
        }[p[2]]
        p[0] = p[1].set_update(p[3], op_mapped);

def p_assignemnt_operator(p):
    """assign_op : ASSIGN
                 | ASSIGN_PLUS
                 | ASSIGN_MINUS
                 | ASSIGN_MUL
                 | ASSIGN_DIV
                 | ASSIGN_MOD
                 | ASSIGN_AND
                 | ASSIGN_OR
                 | ASSIGN_XOR"""
    p[0] = p[1]

def p_statement_del(p):
    "statement : DEL expression_4 SEMICOLON"
    if type(p[2]) is not LValue:
        print("Error: left hand side of assignment must be an lvalue")
        raise SyntaxError
    p[0] = p[2].del_()

def p_statement_if(p):
    "statement : IF expression_4 LBRACE statement_list RBRACE if_tail"
    p[0] = if_body(p[2], p[4], p[6])

def if_body(condition, body, tail):
    result = condition.get()
    lbl_tail = object()
    lbl_end = object()
    result.append(Linkable(bytes([OPCODES['OP_NOT'], OPCODES['JUMP_IF'], 0,0,0,0]), relocations={2: lbl_tail}))
    result.append(body)
    result.append(Linkable(bytes([OPCODES['JUMP'], 0,0,0,0]), relocations={1: lbl_end}))
    result.symbols[lbl_tail] = len(result)
    result.append(tail)
    result.symbols[lbl_end] = len(result)
    return result

def p_statement_if_tail(p):
    """if_tail : ELIF expression_4 LBRACE statement_list RBRACE if_tail
             | ELSE LBRACE statement_list RBRACE
             | """ # empty
    if len(p) == 1:
        p[0] = Linkable(b'')
    elif len(p) == 5:
        p[0] = p[3]
    else:
        p[0] = if_body(p[2], p[4], p[6])

def p_statement_while(p):
    "statement : WHILE expression_4 LBRACE statement_list RBRACE"
    p[0] = p[2].get()
    lbl_end = object()
    lbl_start = object()
    p[0].symbols[lbl_start] = 0
    p[0].append(Linkable(bytes([OPCODES['OP_NOT'], OPCODES['JUMP_IF'], 0,0,0,0]), relocations={2: lbl_end}))
    p[0].append(p[4])
    p[0].append(Linkable(bytes([OPCODES['JUMP'], 0,0,0,0]), relocations={1: lbl_start}))
    p[0].symbols[lbl_end] = len(p[0])

    for addr, lbl in p[0].relocations.items():
        if lbl is l_CONTINUE:
            p[0].relocations[addr] = lbl_start
        elif lbl is l_BREAK:
            p[0].relocations[addr] = lbl_end

def p_statement_for(p):
    "statement : FOR IDENT IN expression_4 LBRACE statement_list RBRACE"
    unique_ident = '__for_%s' % p.lexpos
    lbl_start = object()
    lbl_end = object()
    lbl_catch = object()
    p[0] = Linkable(OPCODES['LIT_BYTES'], encode_bytes(unique_ident))
    p[0].append(p[4].get())
    p[0].append(Linkable(
        OPCODES['LIT_BYTES'],
        encode_bytes("__iter__"),
        OPCODES['GET_ATTR'],
        OPCODES['TUPLE_0'],
        OPCODES['CALL'],
        OPCODES['SET_LOCAL'],
    ))
    p[0].symbols[lbl_start] = len(p[0])
    p[0].append(Linkable(OPCODES['TRY'], 0,0,0,0, relocations={1: lbl_catch}))
    p[0].append(Linkable(
        OPCODES['LIT_BYTES'],
        encode_bytes(p[2]),
        OPCODES['LIT_BYTES'],
        encode_bytes(unique_ident),
        OPCODES['GET_LOCAL'],
        OPCODES['LIT_BYTES'],
        encode_bytes("__next__"),
        OPCODES['GET_ATTR'],
        OPCODES['TUPLE_0'],
        OPCODES['CALL'],
        OPCODES['SET_LOCAL'],
        OPCODES['TRY_END'],
    ))
    p[0].append(p[6])
    p[0].append(Linkable(OPCODES['JUMP'], 0,0,0,0, relocations={1: lbl_start}))
    p[0].symbols[lbl_catch] = len(p[0])
    p[0].append(Linkable(OPCODES['RAISE_IF_NOT_STOP']))
    p[0].symbols[lbl_end] = len(p[0])

    for addr, lbl in p[0].relocations.items():
        if lbl is l_CONTINUE:
            p[0].relocations[addr] = lbl_start
        elif lbl is l_BREAK:
            p[0].relocations[addr] = lbl_end

def p_statement_return(p):
    "statement : RETURN expression_4 SEMICOLON"
    p[0] = p[2].get().append(Linkable(bytes([OPCODES['RETURN']])))

def p_statement_throw(p):
    "statement : THROW expression_4 SEMICOLON"
    p[0] = p[2].get().append(Linkable(bytes([OPCODES['RAISE']])))

def p_statement_yield(p):
    "statement : YIELD expression_4 SEMICOLON"
    p[0] = p[2].get().append(Linkable(bytes([OPCODES['YIELD']])))

def p_statement_label(p):
    "statement : IDENT COLON"
    p[0] = Linkable(b'', symbols={p[1]: 0})

def p_statement_goto(p):
    "statement : GOTO IDENT SEMICOLON"
    p[0] = Linkable(bytes([OPCODES['JUMP'], 0,0,0,0]), relocations={1: p[2]})

def p_statement_continue(p):
    "statement : CONTINUE SEMICOLON"
    p[0] = Linkable(bytes([OPCODES['JUMP'], 0,0,0,0]), relocations={1: l_CONTINUE})

def p_statement_break(p):
    "statement : BREAK SEMICOLON"
    p[0] = Linkable(bytes([OPCODES['JUMP'], 0,0,0,0]), relocations={1: l_BREAK})

def p_statement_try(p):
    "statement : TRY LBRACE statement_list RBRACE CATCH IDENT LBRACE statement_list RBRACE"
    lbl_catch = object()
    lbl_end = object()
    p[0] = Linkable(bytes([OPCODES['TRY'], 0,0,0,0]), relocations={1: lbl_catch})
    p[0].append(p[3])
    p[0].append(Linkable(bytes([OPCODES['TRY_END'], OPCODES['JUMP'], 0,0,0,0]), relocations={2: lbl_end}))
    p[0].symbols[lbl_catch] = len(p[0])
    p[0].append(Linkable(bytes([OPCODES['LIT_BYTES']]) + encode_bytes(p[6]) + bytes([OPCODES['ST_SWAP'], OPCODES['SET_LOCAL']])))
    p[0].append(p[8])
    p[0].symbols[lbl_end] = len(p[0])
