int isprint(int x) {
    return x >= ' ' && x < 255;
}

int islower(int x) {
    return x >= 'a' && x <= 'z';
}

int isupper(int x) {
    return x >= 'A' && x <= 'Z';
}

int isspace(int x) {
    return x == ' ' || x == '\n' || x == '\t';
}

int isdigit(int x) {
    return x >= '0' && x <= '9';
}

int isxdigit(int x) {
    return (x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f');
}

int toupper(int x) {
    return islower(x) ? (x - 'a' + 'A') : x;
}

int tolower(int x) {
    return isupper(x) ? (x - 'A' + 'a') : x;
}
