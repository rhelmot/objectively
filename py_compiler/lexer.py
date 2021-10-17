import re
from ply.lex import TOKEN

keywords = {
        'fn': 'FN',
        'class': 'CLASS',
        'del': 'DEL',
        'if': 'IF',
        'elif': 'ELIF',
        'else': 'ELSE',
        'while': 'WHILE',
        'try': 'TRY',
        'catch': 'CATCH',
        'and': 'LOGAND',
        'or': 'LOGOR',
        'return': 'RETURN',
        'throw': 'THROW',
        'goto': 'GOTO',
        'continue': 'CONTINUE',
        'break': 'BREAK',
}
punctuation = {
        '.': 'DOT',
        ',': 'COMMA',
        ';': 'SEMICOLON',
        ':': 'COLON',
        '+': 'PLUS',
        '-': 'MINUS',
        '*': 'TIMES',
        '/': 'DIV',
        '%': 'MOD',
        '&': 'BITAND',
        '|': 'BITOR',
        '^': 'XOR',
        '!': 'NOT',
        '~': 'INV',
        '(': 'LPAREN',
        ')': 'RPAREN',
        '{': 'LBRACE',
        '}': 'RBRACE',
        '[': 'LBRACKET',
        ']': 'RBRACKET',
        '=': 'ASSIGN',
        '+=': 'ASSIGN_PLUS',
        '-=': 'ASSIGN_MINUS',
        '*=': 'ASSIGN_MUL',
        '/=': 'ASSIGN_DIV',
        '%=': 'ASSIGN_MOD',
        '&=': 'ASSIGN_AND',
        '|=': 'ASSIGN_OR',
        '^=': 'ASSIGN_XOR',
        '==': 'EQ',
        '!=': 'NE',
        '>': 'GT',
        '<': 'LT',
        '>=': 'GE',
        '<=': 'LE',
        '<<': 'SHL',
        '>>': 'SHR',
}
tokens = (
        'LIT_INT',
        'LIT_FLOAT',
        'LIT_BYTES',
        'IDENT',
) + tuple(keywords.values()) + tuple(punctuation.values())

for op, name in punctuation.items():
    globals()[f't_{name}'] = re.escape(op)

m_LIT_INT = r'[0-9]+(?![\.x])|0x[0-9a-fA-F]+'
m_LIT_FLOAT = r'([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)'
m_LIT_BYTES = r'"([^"\\]*(\\\\|\\"|\\' + "'" + r'|\\n|\\r|\\t|\\x[0-9a-fA-F]{2})*)*"|' + \
              r"'([^'\\]*(\\\\|\\'|\\" + '"' + r"|\\n|\\r|\\t|\\x[0-9a-fA-F]{2})*)*'"
@TOKEN(m_LIT_INT)
def t_LIT_INT(t):
    t.value = int(t.value, 0)
    return t
@TOKEN(m_LIT_FLOAT)
def t_LIT_FLOAT(t):
    t.value = float(t.value)
    return t
@TOKEN(m_LIT_BYTES)
def t_LIT_BYTES(t):
    t.value = eval('b' + t.value)
    return t
def t_IDENT(t):
    r"[_a-zA-Z][_a-zA-Z0-9]*"
    t.type = keywords.get(t.value, 'IDENT')
    return t
def t_COMMENT(t):
    r'\#.*'
    pass
t_ignore = ' \t'
def t_newline(t):
    r'\n+'
    t.lexer.lineno += len(t.value)
