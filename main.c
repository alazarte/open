#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>

#define BIG_BUFFER_SIZE 1000
#define BUFFER_SIZE 500

typedef enum FILETYPE {
	F_PGP,
	F_ASCII,
	F_PDF,
	F_MP4,
	F_IMG,
	F_HTML,
	F_DIR,
	F_DEFAULT,
} FileType;

const char *md5_check_command = "md5sum %s | cut -d' ' -f1";
const char *gpg_decrypt_file_command = "gpg -d -r %s %s > %s\n";
const char *gpg_encrypt_file_command = "gpg -e -r %s -o %s %s\n";
const char *pdf_reader_command = "zathura";
const char *media_player_command = "mpv";
// TODO incomplete command
const char *image_viewer_command =
    "feh -e \"yudit/13\" -. -B \"#000000\" -d --edit";
const char *file_explorer_command = "pcmanfm";

int run_system(char *command)
{
	int status = system(command);
	printf("(%d) => %s\n", status, command);
	return status;
}

const char *get_filename_ext(const char *filename)
{
	const char *dot = strrchr(filename, '.');
	if (!dot || dot == filename)
		return "";
	return dot + 1;
}

int run_save_stdout(char *command, char *output)
{
	FILE *fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command\n");
		return 1;
	}

	printf("%s\n", fgets(output, BUFFER_SIZE, fp));

	int status = pclose(fp);
	printf("(%d) => %s\n", status, command);

	return status;
}

void sanitize_filename(char *filename, char *sanitized)
{
	while (*filename != '\0') {
		if ((*filename >= 'a' && *filename <= 'z') ||
		    (*filename >= 'A' && *filename <= 'Z') ||
		    (*filename >= '0' && *filename <= '9') ||
		    *filename == '/' || *filename == '.')
			*sanitized++ = *filename;
		filename++;
	}
}

void open_encrypted_and_reencrypt(char *filename)
{
	char tmp_filename[BUFFER_SIZE] = { 0 };
	char buffer[BIG_BUFFER_SIZE] = { 0 };

	if (run_save_stdout("mktemp", tmp_filename) != 0) {
		printf("Command failed!\n");
		return;
	}

	char sanitized[BUFFER_SIZE] = { 0 };
	sanitize_filename(tmp_filename, sanitized);
	sprintf(buffer, gpg_decrypt_file_command, getenv("GPG_KEY"), filename,
		sanitized);
	if (run_system(buffer) != 0) {
		printf("Command failed!\n");
		return;
	}
	// get hash of the decrypted file before opening
	char original_hash[100];
	sprintf(buffer, md5_check_command, sanitized);
	if (run_save_stdout(buffer, original_hash) != 0) {
		printf("Command failed!\n");
		return;
	}

	sprintf(buffer, "open %s", sanitized);
	if (run_system(buffer) != 0) {
		printf("Command failed!\n");
		return;
	}

	char final_hash[100];
	sprintf(buffer, md5_check_command, sanitized);
	if (run_save_stdout(buffer, final_hash) != 0) {
		printf("Command failed!\n");
		return;
	}

	if (strcmp(original_hash, final_hash) != 0) {
		sprintf(buffer, gpg_encrypt_file_command, getenv("GPG_KEY"),
			filename, sanitized);
		if (run_system(buffer) != 0) {
			printf("Command failed!\n");
			return;
		}
	} else {
		printf("Not reencrypting, files are the same\n");
	}
}

FileType check_file_type(char *filename)
{
	const char *ext = get_filename_ext(filename);
	printf("file ext: %s\n", ext);
	// file type by extension
	if (strcmp(ext, "html") == 0) {
		return F_HTML;
	}

	char buffer[BIG_BUFFER_SIZE] = { 0 };
	sprintf(buffer, "file -b \"%s\"", filename);

	char output[BUFFER_SIZE] = { 0 };
	if (run_save_stdout(buffer, output) != 0) {
		printf("Command failed!\n");
		return F_DEFAULT;
	}

	printf("type:%s\n", output);

	if (strstr(output, "ASCII") != NULL ||
	    strstr(output, "Unicode") != NULL
	    || strstr(output, "empty") != NULL) {
		return F_ASCII;
	} else if (strstr(output, "PGP") != NULL || strstr(output, "GPG")) {
		open_encrypted_and_reencrypt(filename);
		return F_PGP;
	} else if (strstr(output, "PDF") != NULL) {
		return F_PDF;
	} else if (strstr(output, "ISO") != NULL) {
		if (strstr(output, "MP4") != NULL) {
			return F_MP4;
		}
	} else if (strstr(output, "PNG") != NULL) {
		return F_IMG;
	} else if (strstr(output, "JPEG") != NULL) {
		return F_IMG;
	} else if (strstr(output, "directory") != NULL) {
		return F_DIR;
	}

	return F_DEFAULT;
}

char valid_url(char *url)
{
	char *url_prefix = "http.://";

	while (*url_prefix != '\0') {
		if (*url_prefix != *url && *url_prefix != '.') {
			return 0;
		}
		url_prefix++;
		url++;
	}

	return 1;
}

void format_final_command(char *final_command, char *fname, char is_url)
{
	char *format_command = "%s \"%s\"";

	if (is_url) {
		sprintf(final_command, format_command, getenv("BROWSER"),
			fname);
		return;
	}

	switch (check_file_type(fname)) {
	case F_HTML:
		sprintf(final_command, format_command, getenv("BROWSER"),
			fname);
		break;
	case F_PGP:
		sprintf(final_command, "echo 'Completed?'");
		break;
	case F_ASCII:
		sprintf(final_command, format_command, getenv("EDITOR"), fname);
		break;
	case F_PDF:
		sprintf(final_command, format_command, pdf_reader_command,
			fname);
		break;
	case F_MP4:
		// TODO shouldn't be an option, should be F_MP4 and play with fplay or something
		sprintf(final_command, format_command, media_player_command,
			fname);
		break;
	case F_IMG:
		sprintf(final_command, format_command, image_viewer_command,
			fname);
		break;
	case F_DIR:
		sprintf(final_command, format_command, file_explorer_command,
			fname);
		break;
	case F_DEFAULT:
		sprintf(final_command, "xdg-open %s", fname);
		break;
	}
}

int main(int argn, char **argv)
{
	if (argn < 2) {
		fprintf(stderr, "Missing filename\n");
		return 1;
	}

	char *fname = argv[1];
	char is_url = 0;

	int stat = access(fname, F_OK);
	if (stat != 0) {
		// TODO check if it's an URL, and parse it
		fprintf(stderr, "File not found\n");
		if (! valid_url(fname)) {
			fprintf(stderr, "Not a valid URL either\n");
			return 1;
		}
		is_url = 1;
	}

	char final_command[BUFFER_SIZE];

	format_final_command(final_command, fname, is_url);

	printf("final_command=%s\n", final_command);
	if (run_system(final_command) != 0) {
		printf("Command failed!\n");
		return 1;
	}

	return 0;
}
