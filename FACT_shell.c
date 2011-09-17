/* This file is part of Furlow VM.
 *
 * FACT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FACT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FACT. If not, see <http://www.gnu.org/licenses/>.
 */

#include <FACT.h>

static void sh_help ();
static void print_num (FACT_num_t);
static void print_scope (FACT_scope_t);

/* Shell commands. All are preceded by a ':' when entered. */
static struct
{
  const char *name;
  void (*func)(void);
} shell_commands[] =
  {
    {
      "help",
      &sh_help
    },
    {
      "registers",
      &Furlow_print_registers
    },
    {
      "state",
      &Furlow_print_state
    },
  };

#define NUM_COMMANDS sizeof (shell_commands) / sizeof (shell_commands[0]) 

static void
sh_help () /* Print out a list of shell comands. */
{
  printf ("?help      Show a list of available commands.\n"
	  "?mode      Swich interpreter mode.\n"
	  "?registers Print the values of the VM's reigsters.\n"
	  "?state     Print the VM's current state.\n");
}

static char *
readline () /* Read a single line of input from stdin. */
{
  int c;
  char *res;
  size_t i;

  /* Perhaps replace this with some ncurses or termios routine,
   * like readline.
   */
  res = NULL;
  for (i = 0; (c = getchar ()) != EOF && c != '\n'; i++)
    {
      if (c == '\\')
	{
	  c = getchar ();
	  if (c != '\n')
	    ungetc (c, stdin);
	  else
	    {
	      i--;
	      continue;
	    }
	}
      
      /* Break on newline. */
      if (c == '\n')
	break;
      
      /* Skip all repeated spaces. */
      if (c == ' ')
	{
	  while ((c = getchar ()) == ' ');
	  if (c == '\n' || c == EOF)
	    goto end;
	  ungetc (c, stdin);
	  c = ' ';
	}
      
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = c;
    }

 end:
  if (res != NULL)
    {
      /* Add the null terminator. */
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = '\0';
    }

  return res;
}

static char *
readstmt (const char *ps1, const char *ps2) /* Read a complete FACT statement. */
{
  int c;
  char *res;
  size_t i;
  size_t hold_nl;
  size_t p_count, b_count, c_count;

  res = NULL;
  p_count = b_count = c_count = 0;
  hold_nl = 0;

  /* Display the initial prompt: */
  printf ("%s ", ps1);
  
  /* Read a statement. */
  for (i = 0; (c = getchar ()) != EOF; i++)
    {
      switch (c)
	{
	case '(':
	  p_count++;
	  break;

	case ')':
	  p_count--;
	  break;

	case '[':
	  b_count++;
	  break;

	case ']':
	  b_count--;
	  break;
	  
	case '{':
	  c_count++;
	  break;
	  
	case '}':
	  c_count--;
	  if (p_count == 0
	      && b_count == 0
	      && c_count == 0)
	    {
	      /* The statement is closed by a '}'. */
	      res = FACT_realloc (res, sizeof (char) * (i + 1));
	      res[i++] = c;
	      goto end;
	    }
	  break;

	case ';':
	  if (p_count == 0
	      && b_count == 0
	      && c_count == 0)
	    {
	      /* The statement is closed by a ';'. */
	      res = FACT_realloc (res, sizeof (char) * (i + 1));
	      res[i++] = c;
	      goto end;
	    }
	  break;
 
	case '#':
	  /* Comment. Ignore the rest of the line. */
	  while ((c = getchar ()) != EOF && c != '\n')
	    ; /* Do nothing. */
	  if (c == EOF)
	    goto end;
	  else
	    ungetc (c, stdin);
	  break;

	case '\n':
	  hold_nl++;
	  if (i != 0)
	    /* There are incompletions, print out the second prompt. */
	    printf ("%s ", ps2);
	  i--;
	  continue; /* Skip allocation. */

	case '?':
	  /* Shell command, default back to readline. */
	  ungetc (c, stdin);
	  return readline ();
	  
	default:
	  break;
	}

      while (hold_nl > 0)
	{
	  res = FACT_realloc (res, sizeof (char) * (i + 1));
	  res[i] = '\n';
	  hold_nl--;
	  i++;
	}
      
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = c;
    }

 end:

  /* Push all unused newlines back. */
  while (hold_nl > 0)
    {
      ungetc ('\n', stdin);
      hold_nl--;
    }
  
  if (res != NULL)
    {
      /* Add the null terminator. */
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = '\0';
    }

  return res;
}

void
FACT_shell (void)
{
  bool mode;
  char *input;
  size_t i;
  size_t curr_line;
  FACT_t *ret_val;
  FACT_tree_t parsed;
  FACT_lexed_t tokenized;

  /* Print shell info and initialize the VM. Eventually these should be moved
   * to the main function.
   */
  printf ("Furlow VM version %s\n", FACT_VERSION);
  Furlow_init_vm ();
  FACT_init_interrupt ();
  FACT_add_BIFs (CURR_THIS);

  curr_line = 1;

  /* Set error recovery. */
  if (setjmp (recover))
    {
      printf ("There was an error: %s\n", curr_thread->curr_err.what);
      return;
    }

  /* Initialize mode, true = FACT, false = BASM. */
  mode = true;
  
  for (;;)
    {
      if (mode) /* FACT mode. */
	input = readstmt ("FACT:",
			  "    |");
      else /* Shell mode. */
	{
	  /* Print the prompt: */
	  printf ("BAS %lu> ", CURR_IP);
	  input = readline ();
	}

      if (input == NULL)
	break;

      /* If it's a shell command, run the cmomand. */
      if (input[0] == '?')
	{
	  if (!strcmp ("mode", input + 1)) /* Mode is not in shell commands list. */
	    {
	      mode = !mode;
	      continue;
	    }
	  
	  /* TODO: add bin search here. */
	  for (i = 0; i < NUM_COMMANDS; i++)
	    {
	      if (!strcmp (shell_commands[i].name, input + 1))
		break;
	    }
	  if (i == NUM_COMMANDS)
	    fprintf (stderr, "No command of name %s, try ?help.\n", input + 1);
	  else
	    shell_commands[i].func ();
	  continue;
	}

      if (mode) /* FACT mode. */
	{
	  /* Tokenize and parse the code. */
	  tokenized = FACT_lex_string (input);
	  tokenized.line = curr_line;

	  /* Go through every token and get to the correct line. */
	  for (i = 0; tokenized.tokens[i].id != E_END; i++)
	    curr_line += tokenized.tokens[i].lines;
	  
	  parsed = FACT_parse (tokenized);
	  if (parsed == NULL) /* There was an error parsing, skip. */
	    continue;
	  FACT_compile (parsed);
	}
      else
	/* Assemble the code. */
	FACT_assembler (input);
	  
      /* Run the code. */
      Furlow_run ();

      if (mode)
	{
	  /* The X register contains the return value of the last expression. */
	  ret_val = Furlow_register (R_X);
	  if (ret_val->type != UNSET_TYPE)
	    {
	      printf ("    $");
	      if (ret_val->type == NUM_TYPE)
		print_num ((FACT_num_t) ret_val->ap);
	      else
		print_scope ((FACT_scope_t) ret_val->ap);
	      printf ("\n");
	      ret_val->type = UNSET_TYPE;
	    }
	}
    }
}

static void
print_num (FACT_num_t val)
{
  size_t i;
  
  if (val->array_up != NULL)
    {
      printf (" [");
      for (i = 0; i < val->array_size; i++)
	{
	  if (i)
	    printf (", ");
	  print_num (val->array_up[i]);
	}
      printf (" ]");
    }
  else
    printf (" %s", mpc_get_str (val->value));
}

static void
print_scope (FACT_scope_t val)
{
  size_t i;
  
  if (*val->array_up != NULL)
    {
      printf (" [");
      for (i = 0; i < *val->array_size; i++)
	{
	  if (i)
	    printf (", ");
	  print_scope ((*val->array_up)[i]);
	}
      printf (" ]");
    }
  else
    printf (" { name = '%s' , code = %lu }", val->name, *val->code);
}

  
/* For testing. To be replaced elsewhere. */
main ()
{
  FACT_shell ();
  return 0;
}
