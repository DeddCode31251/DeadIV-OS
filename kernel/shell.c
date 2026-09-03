/* =============================================================================
 * shell.c - DeadIV OS's simple interactive command shell
 * =============================================================================
 * This is the "userland" experience of DeadIV OS - the thing a person
 * actually sees and types into. It is deliberately simple: it collects
 * characters the keyboard driver hands it into a line buffer, and when
 * Enter is pressed, compares the typed text against a small list of known
 * commands and runs one.
 *
 * This is NOT a real process/program loader (that's a much bigger feature
 * - see the README's "where to go from here" section) - it's a fixed set
 * of built-in commands compiled directly into the kernel, which is exactly
 * how the very first hobby OS shells (and even early real ones) worked.
 * ===========================================================================
 */
#include "shell.h"
#include "screen.h"
#include "string.h"

#define INPUT_BUFFER_SIZE 128
#define PROMPT "deadiv> "

static char input_buffer[INPUT_BUFFER_SIZE];
static u32  input_length = 0;

static void print_prompt(void)
{
    screen_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print_string(PROMPT);
    screen_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
}

/* run_command: look at the fully-typed line in input_buffer and execute
 * whichever built-in command it matches. Add new commands here! */
static void run_command(const char *cmd)
{
    if (k_strlen(cmd) == 0) {
        return; /* empty line - do nothing */
    }

    if (k_strcmp(cmd, "help") == 0) {
        print_string("Available commands:\n");
        print_string("  help    - show this list\n");
        print_string("  about   - about DeadIV OS\n");
        print_string("  clear   - clear the screen\n");
        print_string("  echo X  - print X back to the screen\n");
    }
    else if (k_strcmp(cmd, "about") == 0) {
        print_string("DeadIV OS - a teaching operating system built from scratch.\n");
        print_string("Bootloader -> Protected Mode -> C kernel -> IDT -> keyboard -> shell.\n");
        print_string("See README.md in the project for a full from-zero explanation.\n");
    }
    else if (k_strcmp(cmd, "clear") == 0) {
        screen_clear();
    }
    else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' &&
             (cmd[4] == ' ' || cmd[4] == '\0')) {
        /* "echo" followed by a space and then whatever the user typed */
        print_string(cmd + 5);
        print_char('\n');
    }
    else {
        print_string("Unknown command: ");
        print_string(cmd);
        print_char('\n');
        print_string("Type 'help' for a list of commands.\n");
    }
}

/* shell_init: print the welcome banner and the first prompt. Called once
 * from kmain() after every subsystem is ready. */
void shell_init(void)
{
    screen_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print_string("=========================================\n");
    print_string("         Welcome to DeadIV OS\n");
    print_string("=========================================\n");
    screen_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print_string("Type 'help' to see available commands.\n\n");

    input_length = 0;
    print_prompt();
}

/* shell_handle_char: called by the keyboard driver once per printable
 * key press. This is the shell's entire "event loop" - there's no
 * separate polling loop; it's purely interrupt-driven. Between key
 * presses, the CPU is doing nothing at all but idling (see kmain's
 * `hlt` loop) - a real OS would be running other tasks in that time. */
void shell_handle_char(char c)
{
    if (c == '\n') {
        print_char('\n');                 /* echo the newline */
        input_buffer[input_length] = '\0'; /* terminate the typed string */
        run_command(input_buffer);
        input_length = 0;                  /* reset for the next line */
        print_prompt();
    }
    else if (c == '\b') {
        if (input_length > 0) {
            input_length--;
            print_char('\b');              /* erase on screen too */
        }
    }
    else {
        if (input_length < INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_length++] = c;
            print_char(c);                 /* echo the typed character */
        }
    }
}
