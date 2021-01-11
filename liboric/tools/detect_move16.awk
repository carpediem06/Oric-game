BEGIN { ldasta_state = 0; old_ldasta_state = 0;}

{old_ldasta_state = ldasta_state;}

{argshift=1; if ($1 ~ "[A-Z]_....") {argshift++; $1="";}}

{if (($argshift != "lda") && ($argshift != "sta"))
    # break the sequence: release the buffered lines
    { if (ldasta_state > 0)  ldasta_state = 0;
    }
}


/lda.*\#/ { if (ldasta_state == 0) { saved_line[0] = $0;
	      gsub("#\\\$","",$(argshift+1));
	      lda_address_1 = $(argshift+1);
	      ldasta_state++;}
   
   if (ldasta_state == 2) { saved_line[2] = $0;
     gsub("#\\\$","",$(argshift+1));
     lda_address_2 = $(argshift+1);
     ldasta_state++; }
}

/sta/{ if (ldasta_state == 1) { saved_line[1] = $0;
	 gsub("\\\$","",$(argshift+1));
	 sta_address_1 = $(argshift+1);
	 ldasta_state++; }
   
   if (ldasta_state == 3) {  saved_line[3] = $0;
     gsub("\\\$","",$(argshift+1));
     sta_address_2 = $(argshift+1);
	 if (sta_address_2 - sta_address_1 == -1) {
	   swp = sta_address_1;
	   sta_address_1 = sta_address_2;
	   sta_address_2 = swp;
	   swp = lda_address_1;
	   lda_address_1 = lda_address_2;
	   lda_address_2 = swp;
	   
	   
	 }
	 if (sta_address_2 - sta_address_1 == 1) {
	   print "line "NR": possible move16\t#$" lda_address_2 "" lda_address_1 ",$" sta_address_1;
	 }
	 ldasta_state = 0; }
}

#{if (old_ldasta_state == ldasta_state) { print; }}
