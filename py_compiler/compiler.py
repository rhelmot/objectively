import sys
from ply.lex import lex
from ply.yacc import yacc
import pprint
import lexer as lexer_module
import parser as parser_module

lexer = lex(module=lexer_module)
parser = yacc(module=parser_module)
result = parser.parse(open(sys.argv[1]).read(), lexer=lexer, debug=False)
open(sys.argv[2], 'wb').write(result.link())
