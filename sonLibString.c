/*
 * sonLibString.c
 *
 *  Created on: 24-May-2010
 *      Author: benedictpaten
 */

#include "sonLibGlobalsPrivate.h"

char *st_string_copy(const char *string) {
	return strcpy(st_malloc(sizeof(char)*(1+strlen(string))), string);
}

char *st_string_print(const char *string, ...) {
	int32_t arraySize = 0;
	static char *cA = NULL;
	//return;
	va_list ap;
	va_start(ap, string);
	int32_t i = vsnprintf(cA, arraySize, string, ap);
	va_end(ap);
	assert(i >= 0);
	if(i >= arraySize) {
		arraySize = i+1;
		if(cA != NULL) {
			free(cA);
		}
		cA = st_malloc(sizeof(char) * arraySize);
		va_start(ap, string);
		i = vsnprintf(cA, arraySize, string, ap);
		assert(i+1 == arraySize);
		va_end(ap);
	}
	//vfprintf(stdout, string, ap);
	return st_string_copy(cA);
}

char *st_string_getNextWord(char **string) {
	while(**string != '\0' && isspace(**string)) {
		(*string)++;
	}
	char *i = *string;
	while(**string != '\0' && !isspace(**string)) {
		(*string)++;
	}
	if((*string) - i > 0) {
		char *cA = memcpy(st_malloc(((*string)-i + 1)*sizeof(char)), i, ((*string)-i)*sizeof(char));
		cA[(*string) - i] = '\0';
		return cA;
	}
	return NULL;
}

static int32_t string_replaceP(char *start, const char *pattern) {
	int32_t i;
	for(i=0; i<strlen(pattern); i++) {
		if(start[i] == '\0' || start[i] != pattern[i]) {
			return 0;
		}
	}
	return 1;
}

char *st_string_replace(const char *originalString, const char *toReplace, const char *replacement) {
	char *i, *k;
	int32_t j;
	char *newString;

	assert(strlen(toReplace) > 0); //Must be non zero length replacement string.

	j=0;
	i=(char *)originalString;
	while(*i != '\0') {
		if(string_replaceP(i, toReplace)) {
			j++;
			i += strlen(toReplace);
		}
		else {
			i++;
		}
	}
	newString = st_malloc(sizeof(char)*(strlen(originalString) + j*strlen(replacement) - j*strlen(toReplace) + 1));
	k=newString;
	i=(char *)originalString;
	while(*i != '\0') {
		if(string_replaceP(i, toReplace)) {
			for(j=0; j<strlen(replacement); j++) {
				*k++ = replacement[j];
			}
			i += strlen(toReplace);
		}
		else {
			*k++ = *i++;
		}
	}
	*k = '\0';
	return newString;
}

char *st_string_join(const char *pad, const char **strings, int32_t length) {
	int32_t i, j, k;
	assert(length >= 0);
	j = strlen(pad) * (length > 0 ? length - 1 : 0) + 1;
	for(i=0; i<length; i++) {
		j += strlen(strings[i]);
	}
	char *cA = st_malloc(sizeof(char) * j);
	j = 0;
	for(i=0; i<length; i++) {
		const char *cA2 = strings[i];
		for(k=0; k<(int32_t)strlen(cA2); k++) {
			cA[j++] = cA2[k];
		}
		if(i+1 < length) {
			for(k=0; k<(int32_t)strlen(pad); k++) {
				cA[j++] = pad[k];
			}
		}
	}
	cA[j] = '\0';
	return cA;
}
