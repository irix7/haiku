/* Writes its arguments, or the lines of its standard input, to the kernel
   debug output, which the gates read on the serial console. For scripts of
   the guest that have no other way to report. */
#include <stdio.h>
#include <string.h>
#include <OS.h>

int
main(int argc, char** argv)
{
	char line[512];
	if (argc > 1) {
		line[0] = '\0';
		for (int i = 1; i < argc; i++) {
			strncat(line, argv[i], sizeof(line) - strlen(line) - 2);
			strncat(line, " ", sizeof(line) - strlen(line) - 1);
		}
		debug_printf("muse_log: %s\n", line);
		return 0;
	}
	while (fgets(line, sizeof(line), stdin) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		debug_printf("muse_log: %s\n", line);
	}
	return 0;
}
