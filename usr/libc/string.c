#include <string.h>

char *strcat(char *dst, const char *src) {
    size_t l = strlen(dst);
    strcpy(dst + l, src);
    return dst;
}

char *strncat(char *dst, const char *src, size_t lim) {
    size_t l = strlen(dst);
    if (l < lim) {
        return dst;
    }
    size_t r = lim - l;
    strncpy(dst + l, src, r);
    return dst;
}

size_t strlen(const char *a) {
    size_t s = 0;
    while (*a++) {
        ++s;
    }
    return s;
}

int strncmp(const char *a, const char *b, size_t n) {
    size_t c = 0;
    for (; c < n && (*a || *b); ++c, ++a, ++b) {
        if (*a != *b) {
            return -1;
        }
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    if (a == b) {
        return 0;
    }
    if (a == NULL || b == NULL) {
        return -1;
    }
    for (; *a || *b; ++a, ++b) {
        if (*a != *b) {
            return 1;
        }
    }
    return 0;
}

char *strcpy(char *dst, const char *src) {
    size_t i;
    for (i = 0; src[i]; ++i) {
        dst[i] = src[i];
    }
    dst[i] = 0;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t lim) {
    size_t i = 0;
    while (i < lim) {
        if (!src[i]) {
            dst[i] = 0;
            return dst;
        }
        dst[i] = src[i];
        ++i;
    }
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char) c) {
            return (char *) s;
        }
        ++s;
    }
    return NULL;
}

char *strrchr(const char *s, int c) {
    ssize_t l = (ssize_t) strlen(s);
    if (!l) {
        return NULL;
    }
    while (l >= 0) {
        if (s[l] == (char) c) {
            return (char *) &s[l];
        }
        --l;
    }
    return NULL;
}

int memcmp(const void *a, const void *b, size_t n) {
    const char *l = a;
    const char *r = b;
    for (; n && *l == *r; --n, ++l, ++r);
    return n ? *l - *r : 0;
}

void *memmove(void *dest, const void *src, size_t n) {
	char *d = dest;
	const char *s = src;

	if (d == s) {
		return d;
	}

	if ((s + n) <= d || (d + n) <= s) {
		return memcpy(d, s, n);
	}

	if (d < s) {
		if (((uintptr_t) s) % sizeof(size_t) == ((uintptr_t) d) % sizeof(size_t)) {
			while (((uintptr_t) d) % sizeof(size_t)) {
				if (!n--) {
					return dest;
				}
				*d++ = *s++;
			}
			for (; n >= sizeof(size_t); n -= sizeof(size_t), d += sizeof(size_t), s += sizeof(size_t)) {
				*((size_t *) d) = *((size_t *) s);
			}
		}
		for (; n; n--) {
			*d++ = *s++;
		}
	} else {
		if (((uintptr_t) s) % sizeof(size_t) == ((uintptr_t) d) % sizeof(size_t)) {
			while (((uintptr_t) (d + n)) % sizeof(size_t)) {
				if (!n--) {
					return dest;
				}
				d[n] = s[n];
			}
			while (n >= sizeof(size_t)) {
				n -= sizeof(size_t);
				*((size_t *) (d + n)) = *((size_t *) (s + n));
			}
		}
		while (n) {
			n--;
			d[n] = s[n];
		}
	}

	return dest;
}

void *memset(void *blk, int v, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) blk)[i] = v;
    }
    return blk;
}

void *memcpy(void *dst, const void *src, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) dst)[i] = ((const char *) src)[i];
    }
    return dst;
}

