#!/bin/awk -f

function write_code(str)
{
  if (code_file != "") {
  print str >> code_file;
  }
}

function write_meminit(str)
{
  if (meminit_file != "")
    {
  print str >> meminit_file;
    }
}
function write_header(str)
{
  if (header_file != "")
    {
  print str >> header_file;
    }
}
function write_data(str)
{
  if (data_file != "")
    {
  print str >> data_file;
    }
}
function hex2dec(s) {
  s = tolower(s);
  n = length(s);
  h = 0
  for (i=1; i<=n; i++)
    {
      j = index("0123456789abcdef", substr(s, i, 1));
      if (j == 0)
	return -1; 
      h = h*16 + j-1;
    }
  return h;
}
BEGIN { code_block = 1;
  one_subroutine_met = 0;
  last_branch = 0;
  message = "// converted by asm2c (C) JOTD 2008\n";
  if (code_file != "") {
  print message > code_file;
  }
  if (header_file != "") {
  print message > header_file;
  }
  if (data_file != "") {
    print message > data_file;
  }
  if (meminit_file != "") {
    print message > meminit_file;
  }

  write_meminit("#include \"oric_machine.h\"");
  write_meminit("#include \""data_file"\"");
  write_meminit("void mem_init()");
  write_meminit("{");
  
  write_code("#include \"opcodes.h\"");
  write_code("#include \"custom.h\"");
  write_code("#include \"constants.h\"");
  write_code("#include \""header_file"\"");
  write_code("#include \""data_file"\"");
  write_code("");
  write_code("void run()");
  write_code("{");
  write_code("}");

  write_header("#ifndef _JIT_6502");
  write_header("#define _JIT_6502");
  write_header("void run();");
}
END {  write_header("#endif");
  write_code("}");
  write_meminit("}");
}

{ sub(";","//"); }

{ if ($1 == ".ba") { $0 = ""; } }
  { if ($1 == ".en") { $0 = ""; } }
  

{ if ($0 ~ "^S_.*")
    {  code_block = 0; by_met = 0;
      offset=$1;
      gsub("S_","",offset);
      write_meminit("memcpy(cpu_get_mem()+0x"offset","$1",sizeof("$1"));");
      write_data("// data block " $1); write_data("");
      $0 = "unsigned char " $1 "[] = {"; }
}
{ if ($1 == ".by") { if (by_met) gsub(".by",","); else gsub(".by","");
    by_met = 1;
    gsub("\\\$","0x");
  }
  else
    { if (by_met) { by_met = 0;  write_data("};")} }
}

{ if ($0 ~ "^[EP]_.*") {

    code_block = 1;
    write_header("void "$1"();");
      
    if (!one_subroutine_met)
      {
	one_subroutine_met = 1;
      }
    else
      {
	write_code("\n}\nend_subroutine\n");
      }
    write_code("subroutine("$1")\n{\n"); $1 = $2; $2 = $3; $3 = "";}
}
# jmp: ((void(*)(void))0x12345678)();
{ if ($0 ~ "^[BJ]_.*") {
    
    code_block = 1;
    write_code("label("$1")");
    $1 = $2; $2 = $3; $3 = "";}
}


{     fix_branch = 0;
    if (code_block == 1)
    {
    opcode = $1;
      opcode_args = $1
    if (opcode != "")
      {
    args = $2;

    if ((opcode == "jmp")  && (args ~ "^P_"))
      {
	opcode = "jmpl";
      }
    if ((opcode != "bit") && (opcode ~ "^b..$") && (args ~ "^P_"))
      {
	fix_branch = 1;

	old_branch = args
        args = "B_GEN_" NR;
	
	# goto a function won't work: avoid
	if (opcode == "bcc")
	  {
	    opcode = "bcs";
	  }
	else if (opcode == "bcs")
	  {
	    opcode = "bcc";
	  }
	else if (opcode == "bne")
	  {
	    opcode = "beq";
	  }
	else if (opcode == "beq")
	  {
	    opcode = "bne";
	  }
	else if (opcode == "bmi")
	  {
	    opcode = "bpl";
	  }
	else if (opcode == "bpl")
	  {
	    opcode = "bmi";
	  }
	else
	  {
	    print "unsupported branch "opcode" "args;
	    fix_branch = 0;
	  }
	
      }
    if ((args == "") || (args ~ "^//"))
      {
	$0 = "\t" opcode ";";
      }
    else
      {
	if (args == "A")
	  {
	    opcode = opcode "_A";
	    args = "";
	  }
	else if (args ~ "^#.*")
	  {
	    opcode = opcode "_imm";
	    gsub("#","",args);
	  }
	else if (args ~ "^\\\(.*")
	  {
	    opcode = opcode "_ind";
	    gsub("[()]","",args);
	  }
	if (args ~ ".*,X")
	  {
	    gsub(",X","",args);
	    opcode = opcode "_X";
	  }
	else if (args ~ ".*,Y")
	  {
	    gsub(",Y","",args);
	    opcode = opcode "_Y";
	  }
	
	gsub("\\\$","0x",args);

	opcode_args = opcode "(a)";
	
	$0 = "\t" opcode "(" args ");";
       }
    if (opcodes_file != "")
      {
        print "#define " opcode_args " trace_opcode(" opcode "); unsupported_opcode("opcode")" >> opcodes_file
      }
    
      }
      write_code($0);
      if (fix_branch)
	{
	  write_code("\tjmpl("old_branch");");
	  write_code("label("args")");
	}
    }
  else
    {
      write_data($0);
    }
}



