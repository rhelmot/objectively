#!/usr/bin/env python3

import sys
from ply.lex import lex
from ply.yacc import yacc
import pprint
import lexer as lexer_module
import parser as parser_module

def oly_compile(in_text, debug=False):
    lexer = lex(module=lexer_module)
    parser = yacc(module=parser_module)
    result = parser.parse(in_text, lexer=lexer, debug=debug)
    return result.link()

if __name__ == '__main__':
    result = oly_compile(open(sys.argv[1]).read(), debug='--debug' in sys.argv)
    open(sys.argv[2], 'wb').write(result)
