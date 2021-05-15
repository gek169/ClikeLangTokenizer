#include "stringutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
right- representing following tokens.
child- representing this node.
left- representing the node preceding in the AST.
identification- what kind of node is this?
*/


char* infilename = NULL;
char* outfilename = "default_out.bin";
FILE* ifile = NULL;
FILE* ofile = NULL;
char* entire_input_file;
unsigned long entire_input_file_len = 0;
strll tokenized = {0};

/*DESCRIPTION-
	parses a single matched pair from allocated text.
	the tree looks like this-
	result
		text: text immediately preceding the brackets.
		left: Null
		child:
			text:
			text between tl and tr.
		right: all text following.
*/
static long strll_len(strll* head){
	long len = 1;
	if(!head) return 0;
	while(head->right) {head = head->right; len++;}
	return len;
}


static void strll_show(strll* current, long lvl){
	{long i; /*strll* current = &tokenized;*/
		for(;current != NULL; current = current->right){
			if(current->text){
				for(i = 0; i < lvl; i++) printf("\t");
				/*printf("TOKEN IS:'%s'\n", current->text);*/
				printf("<TOK>%s</TOK>\n",current->text);
			}
			if(current->left)
			{	for(i = 0; i < lvl; i++) printf("\t");
				printf("<LCHILDREN>\n");
				strll_show(current->left, lvl + 1);
				for(i = 0; i < lvl; i++) printf("\t");
				printf("</LCHILDREN>\n");
			}
			if(current->child)
			{	for(i = 0; i < lvl; i++) printf("\t");
				printf("<CHILDREN>\n");
				strll_show(current->child, lvl + 1);
				for(i = 0; i < lvl; i++) printf("\t");
				printf("</CHILDREN>\n");
			}
		}
	}
}
static void strll_recursive_parse_matched(strll* current_meta, char* l, char* r){
	{
		for(;current_meta != NULL && current_meta->text != NULL && current_meta->text[0] != '\"';
			current_meta = current_meta->right)
		{
				parse_matched(current_meta, l, r);
				if(current_meta->left)
					strll_recursive_parse_matched(current_meta->left, l, r);
				if(current_meta->child)
					strll_recursive_parse_matched(current_meta->child, l, r);
		}
	}
}

static char isUnusual(char x){
	if(!isalnum(x) && x != '_')
		return 1;
	return 0;
}
static char isPartOfPair(char x){
	if(
		x == '(' || x == ')' ||
		x == '{' || x == '}'
	)
		return 1;
	return 0;
}

static void tokenizer(strll* work){
	const char* STRING_BEGIN = 		"\"";
	const char* STRING_END = 		"\"";
	const char* CHAR_BEGIN = 		"\'";
	const char* CHAR_END = 			"\'";
	const char* COMMENT_BEGIN = 	"$";
	const char* COMMENT_END = 		"$";
	long mode = 0; long i = 0;
	for(;i < (long)strlen(work->text); i++){
		done_selecting_mode:;
		if(mode != 2 && mode != 3 && mode != 4){ /*Not in a string or char literal or comment.*/
			
		}
		if(mode == -1){/*Determine what our next mode should be.*/
			if(i != 0){
				puts("Something quite unusual has happened... mode is negative one, but i is not zero!");
			}
			if(strfind(work->text, STRING_BEGIN) == 0){
				mode = 2;
				i+= strlen(STRING_BEGIN);
				goto done_selecting_mode;
			}
			if(strfind(work->text, COMMENT_BEGIN) == 0){
				mode = 3;
				i+= strlen(COMMENT_BEGIN);
				goto done_selecting_mode;
			}
			if(strfind(work->text, CHAR_BEGIN) == 0){
				mode = 4;
				i+= strlen(CHAR_BEGIN);
				goto done_selecting_mode;
			}
			if(isspace(work->text[i]) && work->text[i] != '\n'){
				mode = 0;
				i++;
				goto done_selecting_mode;
			}
			if(isUnusual(work->text[i])){ /*Merits its own thing.*/
				work = consume_bytes(work, 1);
				i = -1;
				continue;
			}
			mode = 1; 
		}
		if(mode == 0){ /*reading whitespace.*/
			if(isspace(work->text[i]) && work->text[i]!='\n') continue; 
			work = consume_bytes(work, i); i=-1; mode = -1; continue;
		}
		if(mode == 1){ /*contiguous non-space characters.*/
			if(
				isspace(work->text[i]) ||
				strfind(work->text+i, STRING_BEGIN) == 0 ||
				strfind(work->text+i, COMMENT_BEGIN) == 0 ||
				strfind(work->text+i, CHAR_BEGIN) == 0 ||
				isUnusual(work->text[i])
			)
			{
				if(i>0)
					work = consume_bytes(work, i); 
				else
					puts("We might be stuck in an infinite loop.");
				i = -1; mode = -1; continue;
			}
		}
		if(mode == 2){ /*string.*/
			if(work->text[i] == '\\' && work->text[i+1] != '\0') {i++; continue;}
			if(strfind(work->text + i, STRING_END) == 0){
				i+=strlen(STRING_END);
				work = consume_bytes(work, i); i = -1; mode = -1; continue;
			}
		}
		if(mode == 3){ /*comment.*/
			if(work->text[i] == '\\' && work->text[i+1] != '\0') {i++; continue;}
			if(strfind(work->text + i, COMMENT_END) == 0){
				i+=strlen(COMMENT_END);
				work = consume_bytes(work, i); i = -1; mode = -1; continue;
			}
		}
		if(mode == 4){ /*char literal.*/
			if(work->text[i] == '\\' && work->text[i+1] != '\0') {i++; continue;}
			if(strfind(work->text + i, CHAR_END) == 0){
				i+=strlen(CHAR_END);
				work = consume_bytes(work, i); i = -1; mode = -1; continue;
			}
		}
	}
}


static strll tokenize_with_escapes(char* alloced_text, const char* token, const char escape){
	strll result = {0}; strll* current;
	long current_token_location;
	long len_token;
	current = &result;
	len_token = strlen(token);
	current_token_location = strfind(alloced_text, token);
	while(current_token_location > -1){
		char* temp;
		if(
			/*There is at least one character before us, and it is the escape character.*/
			current_token_location > 0 && 
			alloced_text[current_token_location - 1] == escape
		){
			long escape_count = 1; long current_token_location_2;
			/*Count the number of escapes. if it's odd? Then this is a */
			for(escape_count = 1;(current_token_location - escape_count) >= 0;escape_count++){
				if(alloced_text[current_token_location-escape_count] != escape) {escape_count--;break;}
			}
			if(escape_count & 1){
				puts("ODD NUMBER OF ESCAPES!!! This is an escaped character.");
				current_token_location_2 = strfind(alloced_text + current_token_location + 1, token);
				/**/
				if(current_token_location_2 == -1){
					/*This is part of the portion which gets thrown away!*/
					break;
				} else {
					/*the basic bitch case.*/
					current_token_location = current_token_location_2 + current_token_location + 1;
					continue;
				}
			} else {
				puts("Even number of escapes. This is not an esacped characer.");
			}
		}
		temp = strcatalloc(alloced_text+ current_token_location + len_token, "");
		current->text = str_null_terminated_alloc(alloced_text,current_token_location);
		STRUTIL_FREE(alloced_text);
		alloced_text = temp;
		current_token_location = strfind(alloced_text, token);
		current->right = STRUTIL_CALLOC(1, sizeof(strll));
		current = current->right;
	}
	/*The last one is discarded.*/
	STRUTIL_FREE(alloced_text);
	return result;
}

int main(int argc, char** argv){
	char* larg;  long i;
	larg = argv[0];
	
	for(i = 1; i < argc; i++){
		if(streq(larg,"-i"))
			infilename = argv[i];
		if(streq(larg,"-o"))
			outfilename = argv[i];
		larg = argv[i];
	}
	if(!infilename){
		puts("<COMPILER ERROR> No input file");
		exit(1);
	}
	if(!outfilename){
		puts("<COMPILER ERROR> No output file");
		exit(1);
	}
	ifile = fopen(infilename, "rb");
	ofile = fopen(outfilename, "wb");
	if(!ifile || !ofile){
		puts("<COMPILER ERROR> Cannot open either the input or output file");
		exit(1);
	}
	entire_input_file = read_file_into_alloced_buffer(ifile, &entire_input_file_len);
	/*if(entire_input_file_len > 0) entire_input_file[entire_input_file_len-1] = '\0';*/
	fclose(ifile); ifile = NULL;
	/*Find all instances of illegal characters and replace them with spaces.*/
	/*
	{long i = 0; long len = strlen(entire_input_file);
		for(i = 0; i < len; i++)
			{
				if(entire_input_file[i])
					if(entire_input_file[i] < 32 || entire_input_file[i] > 126){
						if(entire_input_file[i] != '\n')
							entire_input_file[i] = ' ';
						else if((i > 0 && entire_input_file[i-1] != '\\')){
							entire_input_file[i] = ';';\
						}
					}
			}
	}
	*/
	/*Force widening of double parentheses groups.*/

	/*
	printf("\n\n");
	puts(entire_input_file);
	printf("\n\n");
	*/
	/*tokenized = parse_matched(entire_input_file, "{", "}");*/
	
	tokenized.text = entire_input_file;
	tokenizer(&tokenized);
	/*tokenized = tokenize_with_escapes(entire_input_file, ";", '\\');*/
	puts("~~~~~AFTER TOKENIZATION~~~~~");
	strll_show(&tokenized, 0);
	puts("~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	{strll* current_meta = &tokenized;
		for(;current_meta != NULL && current_meta->text != NULL; current_meta = current_meta->right)
		{
	
			strll* current = current_meta;
			strll* retval = current_meta;
			do{
				current = retval;
				current_meta = retval;
				if(current->text){
					 /*long loc_next_semicolon;*/
					long loc_firstquote = strfind(current->text, "\"");
					/*loc_next_semicolon = strfind(current->text, ";");*/
					/*loc_next_semicolon = -1;*/
					if(loc_firstquote > -1)
					{ long ii;
						if(loc_firstquote > 0){
							retval = consume_bytes(current, loc_firstquote);
							current = retval;
							current_meta = retval;
						} else {}
						for(ii = 1; current->text[ii]!= '\0'; ii++){
							if(current->text[ii] == '\\' && current->text[ii] != '\0'){
								ii++;continue; 
							} else if(current->text[ii] == '\\' && current->text[ii] == '\0'){
								puts("ERROR: unmatched quote group.");
								exit(1);
							}
							if(current->text[ii] == '\"'){
								break;
							}
						}
						if(current->text[ii] != '\"'){
							puts("ERROR: unmatched quote group.");
							exit(1);
						}
						retval = consume_bytes(current, ii+1); 
						current = retval;
						current_meta = retval;
					}
				} else {
					puts("Internal Error.");
					exit(1);
				}
			}while(retval != current);
		}
	}
	{strll* current_meta = &tokenized;
		for(;current_meta != NULL && current_meta->text != NULL; current_meta = current_meta->right){
			if(isspace(current_meta->text[0]) && current_meta->text[0] != '\n'){
				/*DEBUGGING PURPOSES... Make sure the whole token is white space.*/
				{
					unsigned long i = 0;for(;i<strlen(current_meta->text);i++){
						if(!isspace(current_meta->text[i]))
						{
							puts("<INTERNAL ERROR> the parser somehow put whitespace in a non-whitespace token.");
							exit(1);
						}
					}
				}
				free(current_meta->text);
				current_meta->text = strcatalloc(" ","");
			}
			if(current_meta->text[0] == '\n'){
				free(current_meta->text);
				current_meta->text = strcatalloc("<newline>","");
			}
		}
	}
	{strll* current_meta = &tokenized;
		for(;current_meta != NULL && current_meta->text != NULL && current_meta->right != NULL && current_meta->right->text != NULL; current_meta = current_meta->right){
			
			while(
			(current_meta != NULL && current_meta->text != NULL && current_meta->right != NULL && current_meta->right->text != NULL) &&
			(strlen(current_meta->right->text) == 0 ||
					(isspace(current_meta->text[0]) &&
					isspace(current_meta->right->text[0])))
			){
				/*pull it out!*/
				strll* kill_me = current_meta->right;
				strll* right_right = current_meta->right->right;
				current_meta->right = right_right;
				free(kill_me->text);
				free(kill_me);
				
			}
		}
	}
	/*
	{strll* current_meta = &tokenized;
			for(;current_meta != NULL && current_meta->text != NULL; current_meta = current_meta->right){
					while(
					current_meta->text && 
							(
								isspace(current_meta->text[0])
							)
					)
					{
						char* text_old = current_meta->text;
						char* text_new = strcatalloc(text_old + 1, "");
						free(text_old);
						current_meta->text = text_new;
					}
					while(
					current_meta->text && 
							(	strlen(current_meta->text) > 0 &&
								isspace(current_meta->text[strlen(current_meta->text)-1])
							)
					)
					{
						char* text_old = current_meta->text;
						char* text_new = str_null_terminated_alloc(text_old, strlen(text_old) - 1);
						free(text_old);
						current_meta->text = text_new;
					}
			}
	}
	*/
	/*strll_recursive_parse_matched(&tokenized, "{", "}");*/
	strll_show(&tokenized, 0);
	return 0;
}
