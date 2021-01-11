#!/bin/env python

import os,sys,re
import getopt,exceptions,traceback,python_lacks
#import count_usage

GLOBAL_SCALAR_STRUCT = "gs"
GLOBAL_ARRAY_STRUCT = "gv"
GLOBAL_POINTER_STRUCT = "gp"
TEMP_STRING_VAR = "str_temp"
DATA_ARRAY = "data_array"

import error_report

class Bas2C:
    __VERSION_NUMBER = "1.0"
    try:
        __MODULE_FILE = sys.modules[__name__].__file__
    except:
        __MODULE_FILE = sys.executable
    __PROGRAM_NAME = os.path.basename(__MODULE_FILE)
    __PROGRAM_DIR = os.path.abspath(os.path.dirname(__MODULE_FILE))

    def __init__(self):
        self.__logfile = ""
        self.__temp_directory = None
        # init members with default values
        self.__case_statement = set()
        self.__output_file = None
        self.__input_file = None
        self.__floats_disabled = False
        self.__logical_disabled = False

        self.__warnings = False
        self.__scalar_variables = set()
        self.__array_variables = dict()
        self.__data_array = list()
        self.__procedures = set()
        self._call_handler_needed = False
        self._goto_handler_needed = False
        self._gosub_handler_needed = False

        try:
            # set binary mode on output/error stream for Windows
            import msvcrt
            msvcrt.setmode (sys.stdout.fileno(), os.O_BINARY)
            msvcrt.setmode (sys.stderr.fileno(), os.O_BINARY)
        except:
            pass
    def __create_temp_directory(self):
        """
        defines self.__temp_directory
        """

        self.__temp_directory = os.path.join(os.getenv("TEMP"),self.__PROGRAM_NAME.replace(".py","")+("_%d" % os.getpid()))
        python_lacks.create_directory(self.__temp_directory)

    def __delete_temp_directory(self):
        if self.__temp_directory != None:
            [rc,output] = python_lacks.rmtree(self.__temp_directory)
            if rc != 0:
                self.__warn("Could not delete temp dir %s: %s" % (self.__temp_directory,output))

    def _init_from_sys_args(self,debug_mode = True):
        """ standalone mode """

        try:
            self.__do_init()
            self.__delete_temp_directory()
        except:
            self.__delete_temp_directory()
            if sys.exc_info()[0] != exceptions.SystemExit:
                if debug_mode:
                    # get full exception traceback
                    traceback.print_exc()
                else:
                    self.__message(python_lacks.ascii_compliant(sys.exc_info()[1].message, best_ascii_approximation=True))
                # log exception by e-mail
                #error_report.ErrorReport()

                sys.exit(1)

# uncomment if module mode is required
##    def init(self,output_file):
##        """ module mode """
##        # set the object parameters using passed arguments
##        self.__output_file = output_file
##        self.__purge_log()
##        self.__doit()

    def __do_init(self):
        #count_usage.count_usage(self.__PROGRAM_NAME,1)
        self.__parse_args()
        self.__purge_log()
        self.__doit()

    def __purge_log(self):
        if self.__logfile != "":
            try:
                os.remove(self.__logfile)
            except:
                pass

    def __message(self,msg,with_prefix=True):
        if with_prefix:
            msg = self.__PROGRAM_NAME+(": %s" % msg)+os.linesep
        else:
            msg += os.linesep
        try:
            sys.stderr.write(msg)
            sys.stderr.flush()
        except:
            # ssh tunneling bug workaround
            pass
        if self.__logfile != "":
            f = open(self.__logfile,"ab")
            f.write(msg)
            f.close()

    def __error(self,msg,user_error=True):
        """
        set user_error to False to trigger error report by e-mail
        """
        error_report.ErrorReport.USER_ERROR = user_error
        raise Exception("*** Error: "+msg+" ***")

    def __warn(self,msg):
        self.__message("*** Warning: "+msg+" ***")

    def __parse_args(self):
        # Command definition
        # Prepare short & long args from list to avoid duplicate info

        # Help's text fields
        self.__opt_string = ""
        self.__opthelp = ""

        parameters = [["version","shows version"],
        ["help","shows this help"],
        ["input=","specify input TAP file"],
        ["output=","specify output BAS file"],
        ["case=","specify lines to insert a case statement: start,stop,step (useful for dynamic goto/gosubs)"],
        ["Floats-disabled","consider all numeric variables as integers"],
        ["Logical-disabled","consider all AND and OR as boolean (&&/||) instead of logical"],
        ]

        # Possible options for user
        options = []

        for oc in parameters:
            # Filling empty shortopts
            if len(oc) == 2:
                # oc[0] is currently the longopt
                # oc[0][0] will be his first letter
                oc.insert(0, oc[0][0])

            # Those variables will mainly be used for display
            short_opt = oc[0]
            long_opt = oc[1]
            desc = oc[2]

            has_args = long_opt.endswith("=")
            if has_args:
                # oc[0] is currently the short opt
                oc[0] += ":"
                long_opt = long_opt[:-1]

            ### HELP TEXT INIT [START] ###
            self.__opt_string += " -"+short_opt+"|--"+long_opt
            if has_args:
                self.__opt_string +=" <>"

            self.__opthelp += "-"+short_opt+" / --"+long_opt+" : "+desc+os.linesep
            ### HELP TEXT INIT [END] ###

            options.append("--"+long_opt)

        # Get all short ops
        short_opts = "".join([param[0] for param in parameters])
        # Get all long opts
        long_opts = [param[1] for param in parameters]
        # Get used opts and args
        used_opts, used_args = getopt.getopt(sys.argv[1:], short_opts, long_opts)

        # Command options
        for option, value in used_opts:
            if value.startswith("-"):
                self.__error("Parameter of %s is also an option (%s). Value omitted" % (option,value))
            oi = 0
            if option in ('-v',options[oi]):
                print(self.__PROGRAM_NAME + " v" + self.__VERSION_NUMBER)
                sys.exit(0)
            oi += 1
            if option in ('-h',options[oi]):
                self.__usage()
                sys.exit(0)
            oi += 1
            if option in ('-i',options[oi]):
                self.__input_file = value
            oi += 1
            if option in ('-o',options[oi]):
                self.__output_file = value
            oi += 1
            if option in ('-c',options[oi]):
                self.__case_statement.add(value)
            oi += 1
            if option in ('-F',options[oi]):
                self.__floats_disabled = True
            oi += 1
            if option in ('-L',options[oi]):
                self.__logical_disabled = True

        if self.__output_file == None:
            self.__error("output file not set")
        if self.__input_file == None:
            self.__error("input file not set")

    def __usage(self):
        sys.stderr.write("Usage: "+self.__PROGRAM_NAME+self.__opt_string+os.linesep)
        sys.stderr.write(self.__opthelp)



    __FLOAT_RE = re.compile(r"([\+\-]?\d*\.\d*E?[\+\-]?\d+|[\+\-]?\d+E[\+\-]?\d+)")
    __INTEGER_RE = re.compile(r"([\+\-]?\d+|#[A-F0-9]+)",re.IGNORECASE)
    __VARIABLE_RE = re.compile(r"([A-Z_][0-9A-Z_]*[\$%]?)")
    __STRING_RE = re.compile(r'"([^"]*)"')
    __BAD_STRING_RE = re.compile(r'"([^"]*)$')

    __LINENUM_RE = re.compile(r"\s*(\d+)")
    __DATA_FIELD_RE = re.compile(r"([^,]*)")

    def __add_data_line(self,data_line):
        data_line = data_line.strip()
        pos=0
        while pos<len(data_line):
            m = self.__STRING_RE.match(data_line,pos)
            if m==None:
                # no quotes
                m = self.__DATA_FIELD_RE.match(data_line,pos)
                if m==None:
                    self.__error("Data parsing error pos %d: '%s'" % (pos,data_line))
                else:
                    d = m.group(1).strip()
            else:
                d = m.group(1)
            self.__data_array.append(d)

            pos = m.end()
            # advance to next comma
            while pos<len(data_line) and data_line[pos]!=',':
                pos+=1
            # skip comma
            pos+=1

    class Token:
        UNKNOWN = 0
        STRING = 1
        SCALAR_VARIABLE = 2
        ARRAY_VARIABLE = 3
        KEYWORD = 4
        INTEGER = 5
        HEX_INTEGER = 6
        FLOAT = 7
        def __init__(self,parent,value,ttype):
            if parent != None:
                self.line = parent._lineno
                self.column = parent._line_pos
                self.__nesting = parent._nesting
            self.value = value
            self.type = ttype
            self.children = [] # child tokens, filled later
            self.procedure = self.type == self.KEYWORD and value in Bas2C.BasicStatement._PROCEDURE_TOKENS
            self.function = self.type == self.KEYWORD and value in Bas2C.BasicStatement._FUNCTION_TOKENS

        def get_nesting(self):
            return self.__nesting

        def is_numeric_constant_type(self):
            return self.type in [self.INTEGER,self.HEX_INTEGER,self.FLOAT]

        def is_string_type(self):
            return isinstance(self.value,str) and (self.type == self.STRING or self.value[-1]=="$" or self.value == "SPC")

        def nest(self,delta=1):
            self.__nesting += delta
            for c in self.children:
                c.nest(delta)

        def nb_children(self,recursive=True):
            rval = len(self.children)
            if recursive:
                for c in self.children:
                    rval += c.nb_children()

            return rval

        def clone(self):
            rval = Bas2C.Token(None,self.value,self.type)
            rval.line = self.line
            rval.column = self.column # will be inexact if clone is inserted before/after this one
            rval.__nesting = self.__nesting
            return rval

        def __str__(self):
            if self.type == self.STRING:
                rval = '"'+str(self.value)+'"'
            elif self.type == self.HEX_INTEGER:
                rval = "0x%X" % self.value
            else:
                rval = str(self.value)

            return rval


    class BasicStatement:
        def __init__(self,parent):
            """
            parent: Bas2C singleton
            """
            self.tokens=[]
            self.parent = parent
        def __str__(self):
            rval = ""
            for t in self.tokens:
                rval += str(t)
            return rval

        def __warn(self,line,message):
            sys.stderr.write("Warning: Line %d: %s\n" % (line,message))
        def __syntax_error(self,token,message):
            line_pos = token.column + len(str(token.line))+2 # adjust to take line number into account
            raw_line = self.__line(token.line).text
            i=0
            for i in xrange(line_pos,len(raw_line)):
                if raw_line[i]!=' ':
                    break
            line_pos = i
            marker = " "*line_pos+"^"
            raise Exception("parse error:\n%s\n%s\n%d:%d: %s" % (raw_line,marker,token.line,line_pos,message))

# utility methods
        def __line(self,lineno=-1):
            if lineno == -1:
                lineno = self.parent._lineno
            if lineno not in self.parent.lines_dict:
                raise Exception("Line %d does not exist" % lineno)
            return self.parent.lines_dict[lineno]

        def __goto_label(self,lineno):
            return "label_%d" % lineno
        def __gosub_procedure(self,lineno):
            return "BP_%05d" % lineno

        def __token_pos(self,tokens,tokpos,tok_value,exception_on_error=False):
            rval = -1
            i = tokpos

            while i < len(tokens):
                if tokens[i].value==tok_value:
                    rval = i
                    break
                i+=1

            if exception_on_error and rval == -1:
                self.__syntax_error(tokens[tokpos-1],"%s expected" % tok_value)

            return rval

        def __nb_tokens_to_end(self,tokens,tokpos):
            else_pos = self.__token_pos(tokens,tokpos,"ELSE")
            if else_pos == -1:
                # no else: discard to the end of the string
                skip=len(tokens)-tokpos
            else:
                # discard until "else"
                skip=else_pos-tokpos
            return skip

#############################
        # each __f_ method gets a t Token in the BasicStatement context
        # it must return a tuple [nb_consumed_tokens,c_token_list]
        # if nb_consumed_tokens == -1 then assume all statement tokens have been consumed (easier)

        def __f_discard(self,tokens,tokpos):
            # discard all remaining tokens from list until ELSE statement or end of line
            return [self.__nb_tokens_to_end(tokens,tokpos),[]]

        def __f_unsupported(self,tokens,tokpos):
            raise Exception("unsupported token %s" % tokens[tokpos-1].value)

        def __f_then(self,tokens,tokpos):
            # special case: "THEN 10"
            rval = []
            has_goto = 0
            t = tokens[tokpos]
            else_pos = self.__token_pos(tokens,tokpos,"ELSE")
            if tokpos==else_pos-1 or tokpos==len(tokens)-1: # last token
                # only supported construction is integer. We omit (t.type == t.SCALAR_VARIABLE) to avoid false alarms
                # like THEN A=10, it will not compile and we'll fix it manually
                if (t.function and not t.value.startswith("_STR")) or t.value=='(' or t.type==t.INTEGER:
                    [gskip,rval] = self.__f_goto(tokens,tokpos)
                    has_goto = 1
                    tokpos += gskip

            rval.extend(self.__translate_tokens(tokens,tokpos,else_pos))
            if else_pos == -1:
                # no else: process the end of the string
                skip=len(tokens)-tokpos
            else:
                # process until "else"
                skip=else_pos-tokpos
            # increase line nesting by 1
            self.__line().nesting += 1

            return [skip+has_goto,rval]

        def __f_if(self,tokens,tokpos):
            # scan until "then"
            then_pos = self.__token_pos(tokens,tokpos,"THEN",True)
            # scan tokens until "then"
            rval = ["if ("]
            rval.extend(self.__translate_tokens(tokens,tokpos,then_pos))
            rval.extend([")\n","{\n"])
            return [then_pos-tokpos,rval]

        def __f_until(self,tokens,tokpos):
            rval = ["\n","}"," ","while","(","!"]
            # like C "while", but with condition reverted!
            rval.extend(self.__translate_tokens(tokens,tokpos))
            rval.append(")")
            return [-1,rval]

        def __f_dim(self,tokens,tokpos):
            rval=[]
            while tokpos<len(tokens):
                if tokens[tokpos].value==",":
                    pass
                else:
                    variable_name = tokens[tokpos].value
                    comma_pos = self.__token_pos(tokens,tokpos,",")
                    variable_size = self.__translate_tokens(tokens[tokpos].children)[1:-1]
                    reworked_variable_size = []
                    fixed_size = True
                    for v in variable_size:
                        if v==",":
                            reworked_variable_size.extend(["]","["])
                        else:
                            if v.isdigit():
                                reworked_variable_size.append(str(int(v)+1))
                            else:
                                fixed_size = False
                                reworked_variable_size.extend([v,"+","1"])

                    if fixed_size:
                        self.parent.add_array_variable(variable_name,reworked_variable_size)
                    else:
                        self.parent.add_pointer_variable(variable_name)
                        if variable_name[-1]=="%":
                            suffix="_int"
                        elif variable_name[-1]=="$":
                            suffix="_str"
                        else:
                            suffix=""
                        rval.extend(["dim"+suffix,"(","&"+GLOBAL_POINTER_STRUCT+"."+variable_name,","]+reworked_variable_size+[")",";","\n"])
                    if comma_pos==-1:
                        break
                    tokpos = comma_pos
                tokpos+=1
            return [-1,rval]

        def __f_for(self,tokens,tokpos):
            #FOR I = 1 TO 3:  1,2,3: => for(i=1;i<=3;i++)
            #FOR I = A TO B STEP C:  for(i=A;(A<B) ? (i<=B) : (i>=B);i+=C)
            C_INTEGER_RE = re.compile(r"[\+\-]?\d+")
            rval = []
            iteration_variable = GLOBAL_SCALAR_STRUCT+"."+tokens[tokpos].value
            tokpos+=2
            to_pos = self.__token_pos(tokens,tokpos,"TO",True)
            start_value_expr = self.__translate_tokens(tokens,tokpos,to_pos)
            limit_expr = self.__translate_token(tokens,to_pos+1)
            step_pos = self.__token_pos(tokens,tokpos,"STEP")
            if (step_pos==-1): # no step
                increment_expr = ["1"]
            else:
                increment_expr = self.__translate_tokens(tokens,step_pos+1)

            # common part
            rval.append("for (")
            rval.append(iteration_variable)
            rval.append("=")
            rval.extend(start_value_expr)
            rval.append(";")

            # simple case:
            if len(start_value_expr)==len(limit_expr)==len(increment_expr)==1 and \
             C_INTEGER_RE.match(start_value_expr[0]) and C_INTEGER_RE.match(limit_expr[0]) \
             and C_INTEGER_RE.match(increment_expr[0]):
                    # simple, constant case
                    start_value = int(start_value_expr[0])
                    limit_value = int(limit_expr[0])
                    increment = int(increment_expr[0])
                    rval.extend(iteration_variable)
                    if start_value<limit_value:
                        rval.append("<=")
                    else:
                        rval.append(">=")
                    rval.extend(limit_expr)
            else:
                # argh complex!
                rval.extend(["("]+start_value_expr+["<"]+limit_expr+[")","?","(",iteration_variable,"<="]+\
                limit_expr+[")",":","(",iteration_variable,">="]+limit_expr+[")"])
            # common part
            rval.append(";")
            rval.extend(iteration_variable)
            if increment_expr==["1"]:
                rval.extend("++")
            elif increment_expr==["-1"]:
                rval.extend("--")
            else:
                rval.extend("+=")
                rval.extend(increment_expr)

            rval.extend([")\n","{\n"])

            return [-1,rval]




        def __f_gosub(self,tokens,tokpos):
            t = tokens[tokpos]

            if t.type == t.INTEGER and self.__nb_tokens_to_end(tokens,tokpos)==1:
                dest_line=t.value
                self.__line(dest_line).gosub = True
                gproc = self.__gosub_procedure(dest_line)
                self.parent.add_gosub_procedure(dest_line)
                rval = [gproc,"(",")"]
                nbskip=1
            else:
                self.parent._gosub_handler_needed = True
                self.__warn(t.line,"unsupported variable/expression GOSUB address")
                rval = ["gosub_handler","("]
                gosub_expr = self.__translate_tokens(tokens,tokpos)
                nbskip=len(gosub_expr)
                rval.extend(gosub_expr)
                rval.append(")")
            return [nbskip,rval]


        def __f_goto(self,tokens,tokpos):
            t = tokens[tokpos]
            if t.type == t.INTEGER and self.__nb_tokens_to_end(tokens,tokpos)==1:
                dest_line=t.value
                self.__line(dest_line).goto = True
                rval = ["goto"," ",self.__goto_label(dest_line)]
                nbskip=1
            else:
                self.parent._goto_handler_needed = True
                self.__warn(t.line,"unsupported variable/expression GOTO address")
                rval = ["goto_handler","("]
                goto_expr = self.__translate_tokens(tokens,tokpos)
                nbskip = len(goto_expr)
                rval.extend(goto_expr)
                rval.append(")")
            return [nbskip,rval]

        def __f_call(self,tokens,tokpos):
            t = tokens[tokpos]
            if t.type in [t.HEX_INTEGER,t.INTEGER]:
                rval = ["AP_%04X()" % t.value]
            else:
                self.parent._call_handler_needed = True
                self.__warn(t.line,"unsupported variable/expression CALL address")
                rval = ["call_handler","("]
                rval.extend(self.__translate_tokens(tokens,tokpos))
                rval.append(")")
            return [1,rval]

        def __f_on(self,tokens,tokpos):
            rval =["switch","(","(","T_INT",")"]
            go_pos = self.__token_pos(tokens,tokpos,"GOTO",False)
            gosub = go_pos==-1
            if gosub:
                # gosub?
                go_pos = self.__token_pos(tokens,tokpos,"GOSUB",True)

            value = self.__translate_tokens(tokens,tokpos,go_pos)
            rval.extend(value)
            rval.extend([")","\n","{"])
            targets = self.__translate_tokens(tokens,go_pos+1)
            i = 1
            for t in targets:
                if t != ",":
                    rval.extend(["\n","case"," ",str(i),":","\n"])
                    dest_line=int(t)
                    if gosub:
                        self.__line(dest_line).gosub = True
                        rval.extend([self.__gosub_procedure(dest_line),"(",")",";","\n"])
                    else:
                        self.__line(dest_line).goto = True
                        rval.extend(["goto"," ",self.__goto_label(dest_line),";","\n"])
                    i += 1
            rval.extend(["\n","default:","\n","break",";","\n","}"])
            return [-1,rval]
        def __f_next(self,tokens,tokpos):
            rval = []
            if tokpos==1:
                # count how many variables the NEXT supports
                nb_vars = self.__nb_tokens_to_end(tokens,tokpos-1) // 2
                if nb_vars==0:
                    nb_vars=1
                for i in xrange(0,nb_vars):
                    rval.extend(["\n","}\n"])
            elif tokens[tokpos-2].value=="THEN":
                rval.append("continue")
                # warn the user: translator is not powerful enough to rework THEN NEXT: other instructions into a
                # valid C for loop with end condition
                # THEN NEXT without anything is easily emulated by a "continue", though.
                self.__warn(tokens[tokpos-1].line,"THEN NEXT: inserted continue, but code may need rewrite/added ')'")
            return [-1,rval]

        def __f_print(self,tokens,tokpos):
            if tokpos==len(tokens):
                # no params: end of line
                return[-1,["print","(","0",")"]]
            if tokens[tokpos]=="@":
                # special print@
                raise Exception("unsupported print@ ATM")
            else:
                return self.__f_print_or_input(tokens,tokpos,"print")

        def __f_input(self,tokens,tokpos):
            return self.__f_print_or_input(tokens,tokpos,"input")

        def __f_print_or_input(self,tokens,tokpos,function_name):
            PRINT_OPERATORS = ["*","/","+","-","("]
            rval = [function_name,"("]
            null_terminator = [",","0",")"]
            else_pos = self.__token_pos(tokens,tokpos,"ELSE")
            if else_pos==-1:
                else_pos=len(tokens)

            last_token_is_semicolon=False
            first_arg = True
            i = tokpos
            while i < else_pos:

                t = tokens[i]
                last_token_is_semicolon = t.value==";"
                if last_token_is_semicolon:
                    # skip
                    pass
                else:
                    if first_arg:
                        first_arg = False
                    else:
                        rval.append(",")

                    # consider argument
                    if t.is_string_type():
                        # simplest case string, string variable or string function: inject as-is
                        rval.extend(self.__translate_token(tokens,i))
                    elif t.value=="TAB":
                        # special case
                        rval.extend(["PRINT_TAB_SEPARATOR",",(const char*)("])
                        i+=1
                        rval.extend(self.__translate_token(tokens,i))
                        rval.append(")")
                    elif t.type in [t.INTEGER,t.HEX_INTEGER,t.FLOAT,t.SCALAR_VARIABLE,t.ARRAY_VARIABLE] or t.function:
                        # integer,float or function result (non-string as ruled out by condition above)
                        # inject, but wrapped by str$() function
                        # problem is print format is permissive: PRINT "hello";A*2 => we want to scan tokens until operator/parenthesis/non-string variable
                        last_i = i+1
                        while last_i<len(tokens):
                            nt = tokens[last_i]
                            if nt.value == "ELSE" or nt.value in PRINT_OPERATORS or nt.type in [t.INTEGER,t.HEX_INTEGER,t.FLOAT,t.SCALAR_VARIABLE,t.ARRAY_VARIABLE] or nt.function:
                                last_i+=1
                            else:
                                break
                        arg = self.__translate_tokens(tokens,i,last_i)
                        i = last_i-1
                        if function_name == "input":
                            # we inject a temporary string and parse it later
                            rval.append(TEMP_STRING_VAR)
                            null_terminator.extend([";","\n"]+arg)
                            if t.value[-1]=="%":
                                conversion_function = 'atoi'
                            else:
                                conversion_function = 'atof'
                            null_terminator.extend(["=",conversion_function,"(",TEMP_STRING_VAR,")"])
                        else:
                            # integer,float or function result (non-string as ruled out by conditions above)
                            # inject, but wrapped by str$() function
                            rval.extend(["str$","("])
                            rval.extend(arg)
                            rval.append(")")
                    elif t.value in ["+","/","*","-"]:
                        rval.append(t.value)
                    elif t.value==",":
                        # insert special separation code
                        rval.append("PRINT_COMMA_SEPARATOR")
                    else:
                        self.__syntax_error(t,"unsupported PRINT parameter %s" % t.value)
                i+=1
            if last_token_is_semicolon:
                if rval[-1]==",":
                    rval.pop()
            else:
                # no semicolon: linefeed
                if function_name != "input":
                    rval.extend([",","PRINT_LINEFEED"])
            rval.extend(null_terminator)
            return [else_pos-tokpos,rval]


        def __f_read(self,tokens,tokpos):
            nb_data = (len(tokens)-tokpos)//2+1
            rval = ["data_read","(",DATA_ARRAY,",","total_nb_data",",",str(nb_data),","]
            for i in xrange(tokpos,len(tokens)):
                t = tokens[i]
                if t.value==',':
                    pass
                else:
                    if t.value[-1]=="%":
                        rval.append("DT_INT")
                        rval.append(",&")
                    elif t.value[-1]=="$":
                        rval.append("DT_STRING")
                        rval.append(",")
                    else:
                        rval.append("DT_NUMERIC")
                        rval.append(",&")

                    rval.extend(self.__translate_token(tokens,i))
                    rval.append(",")
            rval[-1]=")"

            return [-1,rval]

        #__COMPARISON_OPS = ["=","<>",">","<","<=",">="]

        def __f_and(self,tokens,tokpos):
            # && or & depending on boolean or logical operation
            # IF A = 0 AND B = 0 => &&
            # A AND B => &

            if self.parent.is_logical_disabled():
                rval = [' && ']
            else:
                rval = [" & "]
            return [0,rval]

        def __f_or(self,tokens,tokpos):
            # && or & depending on boolean or logical operation
            # IF A = 0 OR B = 0 => ||
            # A OR B => |

            if self.parent.is_logical_disabled():
                rval = [' || ']
            else:
                rval = [" | "]
            return [0,rval]

        def __f_equals(self,tokens,tokpos):
            # equality/affectation for everything but string

            #print "FEQUALS",tokens[tokpos].value,self.__dump_tree_r(tokens[tokpos].children,False)
            if tokpos == 2 or tokens[tokpos-3].value in ["LET","THEN","FOR","ELSE"]:
                # affectation
                rval = [' = ']
            else:
                # test
                rval = [" == "]
            return [0,rval]

        def __f_str_equals(self,tokens,tokpos):
            # equality/affectation for strings
            rval = []

            #print "FEQUALS",tokens[tokpos].value,self.__dump_tree_r(tokens[tokpos].children,False)
            if tokpos == 1 or tokens[tokpos-2].value in ["LET","THEN","FOR"]:
                # affectation
                rval = ["strcpy"]
            else:
                # test
                rval = ["streq"]
            rval.extend(self.__translate_tokens(tokens[tokpos-1].children))
            return [0,rval]


        # None: same value lowercase / unexploited
        # empty string: ignore
        # another string: function name
        # python method: special case

        _KEYWORDS_DICT = {"ABS" : None,
        "AND" : __f_and,
        "ASC" : "asc",
        "ATN" : "atan",
        "AUTO" : __f_unsupported,
        "CALL" : __f_call,
        "CHAR" : "hires_draw_char",
        "CHR$" : None,
        "CIRCLE" : None,
        "CLEAR" : None,
        "CLOAD" : __f_unsupported,
        "CLS" : None,
        "CONT" : __f_unsupported,
        "COS" : None,
        "CSAVE" : __f_unsupported,
        "CURMOV" : None,
        "CURSET" : None,
        "DATA" : None,
        "DEEK" : None,
        "DEF" : __f_unsupported,
        "DIM" : __f_dim,
        "DOKE" : None,
        "DRAW" : None,
        "EDIT" : __f_unsupported,
        "ELSE" : [";","\n","}\n","else","\n","{\n"],#";\n}\nelse\n{\n",
        "END" : None,
        "EXP" : None,
        "EXPLODE" : None,
        "FALSE" : "ORIC_FALSE",
        "FILL" : None,
        "FN" : __f_unsupported,
        "FOR" : __f_for,
        "FRE" : __f_unsupported,
        "GET" : None,
        "GOSUB" : __f_gosub,
        "GOTO" : __f_goto,
        "GRAB" : __f_discard,
        "HEX$" : None,
        "HIMEM" : __f_discard,
        "HIRES" : None,
        "IF" : __f_if,
        "INK" : None,
        "INPUT" : __f_input,
        "INT" : "integer",
        "KEY$" : "key$()",
        "LEFT$" : None,
        "LEN" : "strlength",
        "LET" : "",
        "LIST" : __f_unsupported,
        "LLIST" : __f_unsupported,
        "LN" : None,
        "LOG" : "log10",
        "LORES" : None,
        "LPRINT" : __f_unsupported,
        "MID$" : None,
        "MUSIC" : None,
        "NEW" : __f_unsupported,
        "NEXT" : __f_next,
        "NOT" : "!",
        "ON" : __f_on,
        "OR" : __f_or,
        "PAPER" : None,
        "PATTERN" : None,
        "PEEK" : None,
        "PI" : "M_PI",
        "PING" : None,
        "PLAY" : None,
        "PLOT" : None,
        "POINT" : None,
        "POKE" : None,
        "POP" : __f_unsupported,
        "POS" : None,
        "PRINT" : __f_print,
        "?" : __f_print,
        "PULL" : __f_discard,
        "READ" : __f_read,
        "RECALL" : __f_unsupported,
        "RELEASE" : __f_discard,
        "REM" : None,
        "REPEAT" : ["do","{\n"],
        "RESTORE" : None,
        "RETURN" : None,
        "RIGHT$" : None,
        "RND" : None,
        "RUN" : __f_unsupported, # could be: CLEAR + GOTO 0/GOTO line
        "SCRN" : None,
        "SGN" : None,
        "SHOOT" : None,
        "SIN" : None,
        "SOUND" : None,
        "SPC" : None,
        "SQR" : "sqrt",
        "STEP" : None,
        "STOP" : __f_unsupported,
        "STORE" : __f_unsupported,
        "STR$" : None,
        "TAB" : None,
        "TAN" : None,
        "TEXT" : None,
        "THEN" : __f_then,
        "TO" : None,
        "TROFF" : __f_discard,
        "TRON" : __f_discard,
        "TRUE" : "ORIC_TRUE",
        "UNTIL" : __f_until,
        "USR" : __f_unsupported,
        "VAL" : None,
        "WAIT" : None,
        "ZAP" : None,
        "=<" : None,
        ">=" : None,
        "<>" : "!=",
        "&" : __f_unsupported,
        "!" : __f_unsupported,
        "+" : None,
        "-" : None,
        "*" : None,
        "/" : None,
        "^" : None,
        "<" : None,
        "<=" : None,
        ">=" : None,
        "=" : __f_equals,
        "'" : None,
        "(" : None,
        ")" : None,
        ";" : "",
        ":" : None,
        "," : None,
        ">" : None,
        # fake basic functions inserted by "fix_tree" method to make
        # conversion to C easier
        "_MID2$" : "mid2$",  # MID$ with only 2 parameters
        "_PLOT_CHAR" : "plot_char",
        "_STR_EQUALS" : __f_str_equals,
        "_STR_NOT_EQUALS" : "strneq",
        "_STR_CONCAT" : "strconcat",
        "_POW" : "pow"
                }

        # procedure needing () when translated to C function calls
        _PROCEDURE_TOKENS = set(["CIRCLE","CLEAR","CLOAD","CHAR","CLS","CONT","CSAVE","CURMOV","CURSET","DEF","DOKE","DRAW","EDIT","END",
"FILL","FN","GRAB","HIMEM","HIRES","INK","LORES","MUSIC","NEW","PAPER","PATTERN","PLAY","PLOT","POINT","POKE","POP","PULL","GET",
"RECALL","RELEASE","RESTORE","RUN","SOUND","STOP","STORE","TEXT","UNTIL","USR","WAIT","ZAP","PING","EXPLODE","SHOOT"]) # ,"GOTO","GOSUB"
        _FUNCTION_TOKENS = set(["ABS","ASC","ATN","CHAR","CHR$","COS","DATA","DEEK","EXP","FRE","GET","HEX$",
"INT","LEFT$","LEN","LIST","LLIST","LN","LOG","LPRINT","MID$","ON","PEEK","POS",
"READ","RIGHT$","RND","SCRN","SGN","SIN","SPC","SQR","STR$","TAB","TAN","VAL","_STR_CONCAT","_MID2$","_POW","CALL"])

        def __translate_tokens(self,tokens,start=0,end=-1,debug=False):
            rval = []
            tokpos = start
            max_tokpos = end

            if max_tokpos==-1:
                max_tokpos=len(tokens)
            while tokpos < max_tokpos:
                t = tokens[tokpos]
                if debug:
                    print("TT=>",tokpos,t.value)
                if t.type == t.KEYWORD:
                    v = self._KEYWORDS_DICT[t.value]
                    if v==None:
                        c_value = t.value.lower()
                        rval.append(c_value)
                        #if t.value in self._PROCEDURE_TOKENS
                        if len(t.children):  # '('
                            rval.extend(self.__translate_tokens(t.children,debug=debug))
                    elif v=="":
                        pass
                    elif isinstance(v,list):
                        c_value = v
                        rval.extend(c_value)
                        if len(t.children):  # '('
                            rval.extend(self.__translate_tokens(t.children,debug=debug))
                    elif isinstance(v,str):
                        c_value = v
                        rval.append(c_value)
                        if len(t.children):  # '('
                            rval.extend(self.__translate_tokens(t.children,debug=debug))
                    else:
                        # function call

                        r = v(self,tokens,tokpos+1)
                        if r == None:
                            self.__f_unsupported(tokens,tokpos+1)
                        [nb_skip,c_tokens] = r
                        if nb_skip==-1:
                            nb_skip=len(tokens)-tokpos
                        tokpos+=nb_skip
                        rval.extend(c_tokens)
                else:
                    string_value = str(t)
                    if t.type in [t.SCALAR_VARIABLE,t.ARRAY_VARIABLE]:
                        if string_value[-1]=="$":
                            string_value = "str_"+string_value[:-1]
                        elif string_value[-1]=="%":
                            string_value = "int_"+string_value[:-1]
                        if t.type == t.ARRAY_VARIABLE:
                            if self.parent.is_pointer_variable(string_value):
                                string_value = GLOBAL_POINTER_STRUCT+"."+string_value
                            else:
                                string_value = GLOBAL_ARRAY_STRUCT+"."+string_value
                        else:
                            string_value = GLOBAL_SCALAR_STRUCT+"."+string_value

                    rval.append(string_value)
                    if t.type == t.ARRAY_VARIABLE:
                        params = t.children

                        # we have to replace () by []
                        nt = params[0] #tokens[tokpos+1]
                        if nt.value!="(":
                            self.__syntax_error(nt,"Array variable %s should be followed by (" % t.value)
                        else:
                            j = 1
                            while j < len(params)-1:
                                next_j = j
                                while True:
                                    if next_j == len(params)-1 or params[next_j].value in [",",")"]:
                                        break
                                    next_j+=1

                                rval.append("[")

                                ttoks = self.__translate_tokens(params,j,next_j)
                                integer_cast = True
                                if len(ttoks)==1 and ttoks[0].isdigit():
                                    # integer: direct evaluation
                                    index=int(ttoks[0])  # was -1
                                    rval.append(str(index))
                                else:
                                    if params[j].value[-1]!="%":
                                        rval.append("(T_INT)")
                                    # not integer: add -1 suffix to correct BASIC array offset,
                                    # but try to reduce in case of final substract/add of an integer
                                    # to avoid clumsy C code
##                                    if len(ttoks)>1 and ttoks[-2]=="+":
##                                        if ttoks[-1]=="1":
##                                            # + 1 => remove
##                                            ttoks = ttoks[:-2]
##                                        elif ttoks[-1].isdigit():
##                                            # + number: => + prev number
##                                            ttoks[-1] = str(int(ttoks[-1])-1)
##                                    elif len(ttoks)>1 and ttoks[-2]=="-":
##                                        if ttoks[-1].isdigit():
##                                            # - number: => - next number
##                                            ttoks[-1] = str(int(ttoks[-1])+1)
##                                    else:
##                                        # can't simplify: just add -1 to expression
##                                        ttoks.extend(["-","1"])
                                    rval.extend(ttoks)
                                rval.append("]")
                                j = next_j+1
                tokpos+=1
            return rval


        def __translate_token(self,tokens,tokpos):
            return self.__translate_tokens(tokens,start=tokpos,end=tokpos+1)

        def translate_to_c(self):
            """
            translate current statement to C
            """
            rval = self.__translate_tokens(self.children)
            return rval

        def compute_tree(self):
            """
            compute "tree" according to nesting
            uses self.tokens as input
            creates self.children as output
            """
            self.children = self.__compute_tree_r(start_index=0,nesting=0)

        def __insert_token(self,token,new_token,after=False,after_children=True):
            """
            insert new_token before/after token in flat tokens list
            (will need tree recomputation in the end)
            """
            i = self.tokens.index(token)
            if after:
                if after_children:
                    i += self.tokens[i].nb_children()  # after = after last child of this token
                i += 1

            self.tokens.insert(i,new_token)

        def __delete_token(self,token,with_children=True):
            """
            insert new_token before token in flat tokens list
            (will need tree recomputation in the end)
            """
            if with_children:
                for t in token.children:
                    self.__delete_token(t)

            i = self.tokens.index(token)
            self.tokens.pop(i)


        def fix_tree(self):
            """
            If token is a procedure, insert parenthesis + increase nesting until next "ELSE" or
            statement end where it insert a closing parenthesis, recompute tree if needed

            Using the existing nesting, fix the following constructions by inserting
            pseudo basic functions in the flat token list (C cannot handle POW nor
            string concatenation/affectation/comparison through operators)

            Then, we'll have to recompute the tree if something has been done

            A$ + "string" + ... => _STRCONCAT(A$,"string",...)
            X ^ Y => _POW(X,Y)
            B$ = "hello" (affectation) => _STRCPY("hello",B$)
            B$ = "hello" (comparison after IF or UNTIL) => _STRCMP("hello",B$)

            """
            #print "TREE",self.__dump_tree(with_levels=True) #temp debug
            needs_recompute = False

            i = 0

            # flat scan for procedures parenthesis wrapping
            while i < len(self.tokens):
                t = self.tokens[i]
                if t.procedure:
                    needs_recompute = True
                    # insert a parenthesis
                    open_paren = t.clone()
                    open_paren.value = "("
                    open_paren.procedure = False
                    self.__insert_token(t,open_paren,after=True)
                    # forward until end of statement/else and increase nesting
                    # in the process

                    i += 1 # skip the newly inserted paren
                    tj = t # in case of no params, should insert ()
                    insert_after = True
                    while i < len(self.tokens):
                        tj = self.tokens[i]
                        if tj.value=="ELSE":
                            insert_after=False
                            break
                        i += 1

                    close_paren = t.clone()
                    close_paren.value = ")"
                    close_paren.type = t.KEYWORD
                    close_paren.procedure = False
                    self.__insert_token(tj,close_paren,after=insert_after)
                i += 1

            if needs_recompute:
                self.compute_tree()

            # tree scan to convert string additions => concat function

            needs_recompute = self.__fix_tree_r(self.children,pass_number=1)

            if needs_recompute:
                self.compute_tree()
                #print "TREE PASS1",self.__dump_tree(with_levels=True) #temp debug

            # tree scan for POW and string equals/affectations
            needs_recompute = self.__fix_tree_r(self.children,pass_number=2)

            if needs_recompute:
                self.compute_tree()
                #print "TREE PASS2",self.__dump_tree(with_levels=True) #temp debug

            # tree scan for POW and string equals/affectations
            while self.__fix_tree_r(self.children,pass_number=3):
                self.compute_tree()

        def __fix_tree_r(self,children,pass_number):
            """
            examine tree and insert in flat list, then recompute tree
            """
            rval = False
            i = 0
            while i < len(children):
                t = children[i]
                if pass_number==1:
                    if t.value == "MID$" and len(t.children)==5:
                        # 2 params only: replace function name
                        t.value = "_MID2$"
                    elif t.value == "PLOT" and not children[i+6].is_string_type():
                        # last param is a non-string: replace function name
                        t.value = "_PLOT_CHAR"
                    elif t.function and len(t.children)!=0 and t.children[0].value!="(":
                        # missing '(' after function: tolerated by BASIC interpreter grrrr
                        # fix does not work ATM
                        open_paren = t.clone()
                        open_paren.value = "("
                        open_paren.type = t.KEYWORD
                        open_paren.nest()
                        open_paren.function = False
                        self.__insert_token(t,open_paren,after=True,after_children=False)
                        rval = True

                if pass_number==3 and t.value=="(":
                    # simplify (number) => number (next 3rd pass will keep on computing)
                    if len(t.children)==2 and t.children[0].is_numeric_constant_type():
                        t.type = t.children[0].type
                        t.value = t.children[0].value
                        t.children = []
                        rval = True

                elif i>0 and i<len(children)-1:
                    previous_t = children[i-1]
                    next_t = children[i+1]
                    if pass_number==3:
                        if t.value in ["/","*","+","-","AND","OR"]:
                            a = previous_t.value
                            b = next_t.value
                            c = None
                            d = None
                            if i<len(children)-2:
                                c = children[i+2].value  # next operator if exists
                            if i>1:
                                d = children[i-2].value  # previous operator if exists
                            #print "SLIDING",d,a,t.value,b,c
                            # evaluate constant expressions like 2*5, 6/7 ...
                            # problem: if not done at this level, C division on integers will be problematic
                            # with constructions like 3/2: in C: 0 in BASIC: 1.5 (and in future python too!!)

                            if previous_t.is_numeric_constant_type() and next_t.is_numeric_constant_type():
                                result = None
                                if t.value=="/":
                                    # reduce
                                    if previous_t.type == t.FLOAT or next_t.type == t.FLOAT:
                                        # normal division
                                        if b==0:
                                            self.__syntax_error("Division by zero",t)
                                        result = a / b
                                        result_type = t.FLOAT
                                    else:
                                        # integer division, but should be float if not integer
                                        if a % b == 0:
                                            result = a // b
                                            result_type = t.INTEGER
                                        else:
                                            result = float(a)/b
                                            result_type = t.FLOAT
                                else:
                                    # other operators: standard computation
                                    if previous_t.type == t.FLOAT or next_t.type == t.FLOAT:
                                        result_type = t.FLOAT
                                    else:
                                        result_type = t.INTEGER
                                    if t.value=="*" and c != "/" and d != "/":
                                        result = a * b
                                    elif t.value=="+" and c not in ["*","/"] and d not in ["*","/"]:
                                        result = a + b
                                    elif t.value=="-" and c not in ["*","/"] and d not in ["*","/"]:
                                        result = a - b
                                    elif t.value=="AND":
                                        result = a & b
                                    elif t.value=="OR":
                                        result = a | b

                                rval = result != None
                                if rval:
                                    # skip 2 next children (not deleted yet in tree)
                                    i += 2

                                    previous_t.value = result
                                    previous_t.type = result_type
                                    # remove operator and right term now that it's solved
                                    self.__delete_token(t)
                                    self.__delete_token(next_t)

                    elif pass_number==2:
                        if previous_t.is_string_type():
                            if t.value in ["=","<>"]:

                                # convert "=" and "<>" to functions for strings
                                eq_token = t.clone()
                                if t.value=="=":
                                    eq_token.value="_STR_EQUALS"
                                else:
                                    eq_token.value="_STR_NOT_EQUALS"
                                rval = True

                                previous_t.nest()
                                next_t.nest()
                                t.nest()
                                t.value = ","

                                eq_token.function = True

                                # non-C compatible operator: replace expression by function call: e1 ^ e2 => _POW(e1,e2)
                                self.__insert_token(previous_t,eq_token,after=False)
                                open_paren = t.clone()
                                open_paren.value = "("
                                self.__insert_token(eq_token,open_paren,after=True)
                                close_paren = t.clone()
                                close_paren.value = ")"
                                self.__insert_token(next_t,close_paren,after=True)
                    elif pass_number==1:

                        if t.value=="^":
                            rval = True

                            t.value = ","
                            pow_token = t.clone()
                            t.nest()
                            previous_t.nest()
                            next_t.nest()
                            pow_token.value = "_POW"
                            pow_token.function = True
                            # non-C compatible operator: replace expression by function call: e1 ^ e2 => _POW(e1,e2)
                            self.__insert_token(previous_t,pow_token,after=False)
                            open_paren = t.clone()
                            open_paren.value = "("
                            self.__insert_token(pow_token,open_paren,after=True)
                            close_paren = t.clone()
                            close_paren.value = ")"
                            self.__insert_token(next_t,close_paren,after=True)

                        elif previous_t.is_string_type() and t.value == "+":
                            # string concatenation: is it the first one?
                            if i<2 or children[i-2].value!=",":  # "+" has become ","
                                # first one
                                cat_token = t.clone()
                                cat_token.value="_STR_CONCAT"

                                rval = True
                                t.value = ","
                                t.nest()
                                previous_t.nest()
                                next_t.nest()
                                cat_token.function = True
                                # non-C compatible operator: replace expression by function call: e1 ^ e2 => _POW(e1,e2)
                                self.__insert_token(previous_t,cat_token,after=False)
                                open_paren = t.clone()
                                open_paren.value = "("
                                self.__insert_token(cat_token,open_paren,after=True)
                            else:
                                # next one
                                t.value = ","
                                t.nest()
                                next_t.nest()

                            if i>len(children)-3 or children[i+2].value!="+":
                                # no more
                                comma_arg = t.clone()
                                comma_arg.value = ","
                                self.__insert_token(next_t,comma_arg,after=True)
                                zero_arg = t.clone()
                                zero_arg.value = "0"
                                zero_arg.type = t.INTEGER
                                self.__insert_token(comma_arg,zero_arg,after=True)
                                close_paren = t.clone()
                                close_paren.value = ")"
                                self.__insert_token(zero_arg,close_paren,after=True)


                if len(t.children) >0:
                    if self.__fix_tree_r(t.children,pass_number):
                        rval = True

                i+=1
            return rval

        def __dump_tree(self,with_levels):
            return self.__dump_tree_r(self.children,with_levels)

        def __dump_tree_r(self,children,with_levels):
            rval = ""
            for t in children:
                rval += str(t.value)
                if with_levels:
                    rval += "[%d]" % t.get_nesting()
                rval +=" "
                if len(t.children):
                    if with_levels:
                        rval += " children <"
                    rval += self.__dump_tree_r(t.children,with_levels)
                    if with_levels:
                        rval += ">"

            return rval

        def __compute_tree_r(self,start_index,nesting):
            tokens = self.tokens
            nb_tokens = len(tokens)
            children = []
            i = start_index
            while i < nb_tokens:
                t = tokens[i]
                if t.get_nesting()==nesting:
                    children.append(t)
                    t.children = []
                    if i<nb_tokens-1 and tokens[i+1].get_nesting()>nesting:
                        t.children = self.__compute_tree_r(start_index=i+1,nesting=nesting+1)
                        i += t.nb_children()  # => also count recursively!!!
                else:
                    break
                i+=1
            return children


    class Line:
        def __init__(self,line):
            self.goto = False
            self.gosub = False
            self.case_statement = False
            self.nesting = 0
            self.line = line
            self.comment = None
            self.statements = []

    class BasicLine(Line):
        def __init__(self,line,text):
            Bas2C.Line.__init__(self,line)
            self.text = text
            self.__c_tokens = []

        def __str__(self):
            rval = str(self.line)+" "
            for s in self.statements:
                rval += str(s)+":"
            rval = rval.rstrip(":")
            if self.comment:
                rval += "' " + self.comment
            return rval

        def translate_to_c_first_pass(self):
            self.__c_tokens = []
            for s in self.statements:
                rval = s.translate_to_c()
                if len(rval):
                    rval += [";"]
                self.__c_tokens.extend(rval)
            if self.comment:
                self.__c_tokens += [" //"+self.comment+" \n"]  # trailing space so no antislash could spoil it
            else:
                 self.__c_tokens += ["\n"]
            for i in xrange(0,self.nesting):
                self.__c_tokens += ["}\n"]


        def __warn(self,msg):
            sys.stderr.write("warning: "+msg+"\n")

        def translate_to_c_second_pass(self):
            if self.goto and self.case_statement:
                self.__warn("goto & case statement on same line %d" % self.line)
            if self.gosub and self.case_statement:
                self.__warn("gosub & case statement on same line %d, will ignore case statement" % self.line)
                self.case_statement = False
            if self.goto and self.gosub:
                self.__warn("goto & gosub on same line %d" % self.line)

            rval = ""
            if self.goto:
                rval = "\nlabel_%d:\n" % self.line
            elif self.gosub:
                rval = "\n}\n\nvoid BP_%05d()\n{\n" % self.line
            if self.case_statement:
                rval = "\ncase %d:\n" % self.line
            if rval != "":
                self.__c_tokens.insert(0,rval)

            return self.__c_tokens

        def add_statement(self,statement):
            statement.compute_tree()

            statement.fix_tree()

            self.statements.append(statement)


    @staticmethod
    def __until_non_char(raw_line,line_pos,c):
        rval = -1
        for i in xrange(line_pos,len(raw_line)):
            if raw_line[i]!=c:
                rval = i
                break
        return rval
    @staticmethod
    def __until_non_space(raw_line,line_pos):
        return Bas2C.__until_non_char(raw_line,line_pos,' ')

    def __syntax_error(self,message):
        line_pos = self._line_pos + len(str(self._lineno))+2 # adjust to take line number into account
        self.__error("%d:%d: %s" % (self._lineno,line_pos,message))

    def __process_line(self):
        raw_line = self.__raw_lines_dict[self._lineno]
        rval = self.BasicLine(self._lineno,raw_line)
        self._line_pos = 0
        self._nesting = 0

        statement = self.BasicStatement(self)

        while True:
            is_kw = False
            # first test keywords
            self._line_pos = self.__until_non_space(raw_line,self._line_pos)
            if self._line_pos==-1:
                # end
                rval.add_statement(statement)
                break
            for kw in self.__KEYWORDS_KEYS:
                kw_pos = raw_line.find(kw,self._line_pos) # no regex here, faster
                if kw_pos == self._line_pos:
                    # basic keyword found
                    t = self.Token(self,kw,ttype=self.Token.KEYWORD)
                    self._line_pos += len(kw)
                    is_kw = True
                    break
            if is_kw:
                if kw in ["REM","'"]:
                    if len(statement.tokens):
                        rval.add_statement(statement)
                        statement = self.BasicStatement(self)
                    rval.comment = raw_line[self._line_pos:]
                    break  # end of processing
                elif kw == "DATA":
                    data_line = raw_line[self._line_pos:]

                    rval.comment = "$DATA$"+data_line
                    self.__add_data_line(data_line)
                    break  # end of processing
                elif kw==":":
                    # next statement
                    if self._nesting!=0:
                        self.__syntax_error(lineno,self._line_pos-1,"parenthesis mismatch (%d)" % self._nesting)
                    rval.add_statement(statement)
                    statement = self.BasicStatement(self)

                else:
                    if kw=="(":
                        # increase nesting from next token
                        self._nesting += 1
                        # if previous token is a function or
                        # an array variable, increase nesting right now
                        if len(statement.tokens)==0:
                            pass
                        else:
                            prev_token = statement.tokens[-1]
                            if prev_token.function or prev_token.type==self.Token.ARRAY_VARIABLE:
                                t.nest()

                    elif kw==")":
                        self._nesting -= 1
                    # store token
                    statement.tokens.append(t)
            else:
                # test variables
                m = self.__VARIABLE_RE.match(raw_line,self._line_pos)
                t = None
                if m != None:
                    value = m.group(1)
                    next_pos = m.end()
                    nsp = self.__until_non_space(raw_line,next_pos)
                    if nsp!=-1 and raw_line[nsp]=='(':
                        # must be an array variable, but we don't declare it now
                        # since we don't know the size, and it has been added on DIM statement
                        t = self.Token(self,value,self.Token.ARRAY_VARIABLE)
                    else:
                        # normal variable
                        self.__scalar_variables.add(value)
                        t = self.Token(self,value,self.Token.SCALAR_VARIABLE)
                else:
                    m = self.__FLOAT_RE.match(raw_line,self._line_pos)
                    if m != None:
                        value = m.group(1)
                        t = self.Token(self,float(value),self.Token.FLOAT)
                    else:
                        m = self.__INTEGER_RE.match(raw_line,self._line_pos)
                        if m != None:
                            value = m.group(1)
                            if value[0]=='#':
                                t = self.Token(self,int(value[1:],16),self.Token.HEX_INTEGER)
                            else:
                                t = self.Token(self,int(value),self.Token.INTEGER)

                        else:
                            m = self.__STRING_RE.match(raw_line,self._line_pos)
                            if m == None:
                                m = self.__BAD_STRING_RE.match(raw_line,self._line_pos)
                            if m != None:
                                value = m.group(1)
                                t = self.Token(self,value,self.Token.STRING)
                            else:
                                self.__syntax_error("unknown token")
                if m != None:
                    statement.tokens.append(t)
                    self._line_pos = m.end()

        return rval

    def add_gosub_procedure(self,procedure_name):
        self.__procedures.add(procedure_name)
    def add_array_variable(self,variable_name,size_expression):
        self.__array_variables[variable_name] = size_expression
    def add_pointer_variable(self,variable_name):
        self.__array_variables[variable_name] = None
    def is_pointer_variable(self,variable_name):
        return variable_name in self.__array_variables and self.__array_variables[variable_name] == None
    def __check_variable_names(self,s,vt):
        vd=dict()
        for i in s:
            ei = i[-1]
            if (vt==self.Token.INTEGER and ei=="%") or (vt==self.Token.FLOAT and ei.isalnum()) or (vt==self.Token.STRING and ei=='$'):
                if len(i)>1:
                    i2 = i[0:2]
                    if i2 not in vd:
                        vd[i2] = []
                    vd[i2].append(i)
        for k,v in vd.iteritems():
            if len(v)>1:
                self.__warn("Variable %s is represented by several names: %s" % (k,",".join(v)))

    def is_logical_disabled(self):
        return self.__logical_disabled

    def __doit(self):
        # main processing here

        lines = python_lacks.read_file(self.__input_file)

        self.__raw_lines_dict = dict()
        self.__line_number = list()

        # build dictionary: line number => raw line
        for l in lines:
            m= self.__LINENUM_RE.match(l)
            if m != None:
                line_number = int(m.group(1))
                rest = l[m.end()+1:]
                self.__raw_lines_dict[line_number] = rest
                self.__line_number.append(line_number)

        self.__KEYWORDS_KEYS = self.BasicStatement._KEYWORDS_DICT.keys()
        self.__KEYWORDS_KEYS.sort(key = lambda entry: len(entry),reverse=True)

        self.lines_dict = dict()
        for lineno in self.__line_number:
            self._lineno = lineno
            rval = self.__process_line()
            if lineno in self.lines_dict:
                self.__error("Duplicate line %d in source" % lineno)
            self.lines_dict[lineno] = rval

        # insert manual "case" statements

        for cs in self.__case_statement:
            cl = cs.split(",")
            if len(cl)!=3:
                self.__error("Case statement syntax error: %s" % cs)
            start = int(cl[0])
            end = int(cl[1])
            step = int(cl[2])
            for lineno in xrange(start,end+step,step):
                if lineno in self.lines_dict:
                     self.lines_dict[lineno].case_statement = True

        for lineno in self.__line_number:
            self._lineno = lineno
            self.lines_dict[lineno].translate_to_c_first_pass()
        # header
        rval = []
        rval.append('#include "oric_basic.h"\n')
        rval.append('#include <stdio.h>\n')
        rval.append('#include <stdlib.h>\n')

        if self.__floats_disabled:
            rval.append("#define T_NUMERIC T_INT\n")

        if len(self.__data_array):
            rval.extend(["static const T_STRING_PTR ",DATA_ARRAY,"[","]","=","\n{\n"])
            first_data=True
            nb_data = 0
            for d in self.__data_array:
                if first_data:
                    first_data=False
                else:
                    rval.append(",")
                rval.append('"%s"' % d)
                if nb_data==10:
                    rval.append("\n")
                    nb_data = 0
                nb_data+=1

            rval.append("\n}")
            rval.append(";\n")
            # we know the size which is len(self.__data_array) but this method allows to modify the number of data in C code afterwards
            rval.extend(["static const int total_nb_data","=","sizeof(",DATA_ARRAY,")/sizeof(T_STRING_PTR)",";\n\n"])

        if len(self.__scalar_variables):
            rval.append("\n\nstatic T_STRING %s;\n\ntypedef struct\n" % TEMP_STRING_VAR)
            rval.append("{")
            for k in self.__scalar_variables:
                if k[-1]=="%":
                    t = "T_INT"
                    v = k[:-1]
                    prefix = "int_"
                elif k[-1]=="$":
                    t = "T_STRING"
                    v = k[:-1]
                    prefix = "str_"
                else:
                    t = "T_NUMERIC"
                    prefix = ""
                    v = k
                rval.extend([t," ",prefix+v,";","\n"])
            rval.append("\n} ScalarGlobals;\n\nScalarGlobals %s;\n\n" % GLOBAL_SCALAR_STRUCT)


        if len(self.__array_variables.keys()):
            array_tokens = []
            pointer_tokens = []
            array_tokens.append("\ntypedef struct\n{\n")
            pointer_tokens.append("\ntypedef struct\n{\n")

            for k,sz in self.__array_variables.iteritems():
                if k[-1]=="%":
                    t = "T_INT"
                    prefix = "int_"
                    v = k[:-1]
                elif k[-1]=="$":
                    t = "T_STRING"
                    prefix = "str_"
                    v = k[:-1]
                else:
                    t = "T_NUMERIC"
                    prefix = ""
                    v = k
                if sz==None:
                    pointer_tokens.extend([t," ","*",prefix+v,";","\n"])
                else:
                    array_tokens.extend([t," ",prefix+v,"["]+sz+["]",";","\n"])
            array_tokens.append("\n} ArrayGlobals;\n\nArrayGlobals %s;\n\n"% (GLOBAL_ARRAY_STRUCT))
            pointer_tokens.append("\n} PointerGlobals;\n\nPointerGlobals %s;\n\n"% (GLOBAL_POINTER_STRUCT))
            rval.extend(array_tokens);
            rval.extend(pointer_tokens);

        rval.append("""
static const char *gsn = "gamesave";
static T_INT load_game()
{
    FILE *f = fopen(gsn,"rb");
    if (f != 0)
    {
	fread(&gv,sizeof(gv),1,f);
	fread(&gs,sizeof(gs),1,f);
	fclose(f);
    }
    return (f != 0);
}
static T_INT save_game()
{
    FILE *f = fopen(gsn,"wb");
    if (f != 0)
    {
	fwrite(&gv,sizeof(gv),1,f);
	fwrite(&gs,sizeof(gs),1,f);
	fclose(f);
    }
     return (f != 0);

}
""")
        procs = list(self.__procedures)
        procs.sort()
        for p in procs:
            rval.extend(["void"," "])
            rval.append("BP_%05d" % p)
            rval.extend(["(",")",";","\n"])

        rval.append("\n")

        rval.append("#define clear() memset(&%s,0,sizeof(%s));memset(&%s,0,sizeof(%s));free_pointers(&%s,sizeof(%s)/sizeof(int *))\n\n" % (GLOBAL_SCALAR_STRUCT,GLOBAL_SCALAR_STRUCT,GLOBAL_ARRAY_STRUCT,GLOBAL_ARRAY_STRUCT,GLOBAL_POINTER_STRUCT,GLOBAL_POINTER_STRUCT))

        if self._goto_handler_needed:
            rval.extend(["static void goto_handler(T_INT line_number)\n","{\n","T_STRING msg;\n",r'sprintf(msg,"GOTO: unsupported dynamic line number %d",line_number);',
            "\nfatal_error(msg);\n","}\n"])
        if self._gosub_handler_needed:
            rval.extend(["static void gosub_handler(T_INT line_number)\n","{\n","switch(line_number)\n","{\n",
            "default:\n","{\n","T_STRING msg;\n",r'sprintf(msg,"GOSUB: unsupported line number %d",line_number);',"\nfatal_error(msg);\n","}\n","}\n","}\n"])
        if self._call_handler_needed:
            rval.extend(["static void call_handler(T_INT address)\n","{\n","switch(address)\n","{\n",
            "default:\n","{\n","T_STRING msg;\n",r'sprintf(msg,"CALL: unsupported address %04X",address);',"\nfatal_error(msg);\n","}\n","}\n","}\n"])

        rval.extend(["void basic_entrypoint()\n","{\n","clear();","\n"])

        for lineno in self.__line_number:
            self._lineno = lineno # useless?
            #last_tokens_return = rval[-2]=="return"

            if self.lines_dict[lineno].gosub:
                return_token = rval[-3]
                if return_token!="return":
                    self.__warn("line %d: gosub target not preceded by RETURN (%s)" % (lineno,return_token))
            rval.extend(["/* %d */ " % lineno])
            rval.extend(self.lines_dict[lineno].translate_to_c_second_pass())

        # warn about variables with same 2 starting letters but different name
        for t in [self.Token.FLOAT,self.Token.INTEGER,self.Token.STRING]:
            self.__check_variable_names(self.__scalar_variables,t)
            self.__check_variable_names(self.__array_variables,t)

        # cosmetic C post-processing:
        # fix useless double "((" "))": ((A+B)) => (A+B). (some will remain if triple ((( or ((A+(A+2)+C))
        # fix useless double ;;
        # fix "}" followed by ";" (the "};" construction exists but not in our generator)
        removal_positions = []
        nb_toks = len(rval)
        for i in xrange(1,nb_toks):
            if i>0:
                simplify=False
                if rval[i]=='(' and rval[i-1]=='(':
                    # find the closing parenthesis
                    simplify = False
                    for j in xrange(i+1,nb_toks-1):
                        if rval[j]==')':
                            if rval[j+1]==')':
                                simplify = True
                            break
                    if simplify:
                        removal_positions.append(i)
                        removal_positions.append(j)
                elif rval[i]==';' and rval[i-1]==';':
                    removal_positions.append(i)
                elif rval[i]==';' and rval[i-1].strip() in ['}','{']:
                    removal_positions.append(i)

        # remove indexes by the end to avoid shifting
        removal_positions.sort(reverse=True)
        for rp in removal_positions:
            rval.pop(rp)

        removal_positions = []
        # pass 2: reduce a = a + 1 => a+=1
        nb_toks = len(rval)
        for i in xrange(0,nb_toks-4):
            t1 = rval[i+1].strip()
            t3 = rval[i+3].strip()
            if t1=='=' and rval[i]==rval[i+2] and t3 in ["*","/","+","-"]:
                if rval[i+4]=="1" and t3 in ["+","-"]:
                    rval[i+1] = t3+t3
                    removal_positions.append(i+4)
                else:
                    rval[i+1]=" "+t3+"= "
                removal_positions.append(i+2)
                removal_positions.append(i+3)

        # remove indexes by the end to avoid shifting
        removal_positions.sort(reverse=True)
        for rp in removal_positions:
            rval.pop(rp)

        rval.append("\n}\n")

        output_handle = open(self.__output_file,"wb")
        level = 0
        previous_t = None
        for t in rval:
            ts = t.strip()
            if ts=='{':
                level += 1
            elif ts=='}':
                level -= 1
            if previous_t and previous_t[-1]=="\n":
                output_handle.write("   "*level)
            previous_t = t
            output_handle.write(t)
        output_handle.close()

if __name__ == '__main__':
    """
        Description :
            Main application body
    """


    o = Bas2C()
    o._init_from_sys_args(debug_mode = True)
    #o.init("output_file")
