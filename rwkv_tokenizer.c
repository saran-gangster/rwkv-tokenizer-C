#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TOKENS 100000
#define MAX_TOKEN_LENGTH 256

typedef struct TrieNode {
    struct TrieNode* children[256];
    int value;
} TrieNode;

typedef struct {
    TrieNode* root;
    unsigned char* idx2token[MAX_TOKENS];
    int token2idx[MAX_TOKENS];
    int num_tokens;
} Tokenizer;

TrieNode* createTrieNode() {
    TrieNode* node = (TrieNode*)calloc(1, sizeof(TrieNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->value = -1;
    return node;
}

void insertTrie(TrieNode* root, const unsigned char* key, int key_length, int value) {
    TrieNode* node = root;
    for (int i = 0; i < key_length; i++) {
        unsigned char c = key[i];
        if (!node->children[c]) {
            node->children[c] = createTrieNode();
        }
        node = node->children[c];
    }
    node->value = value;
}

int findLongest(TrieNode* root, const unsigned char* data, int data_length, int* endIndex) {
    TrieNode* node = root;
    int index = 0;
    *endIndex = 0;
    int value = -1;
    while (index < data_length && node->children[data[index]]) {
        node = node->children[data[index]];
        index++;
        if (node->value != -1) {
            *endIndex = index;
            value = node->value;
        }
    }
    return value;
}

Tokenizer* createTokenizer() {
    Tokenizer* tokenizer = (Tokenizer*)calloc(1, sizeof(Tokenizer));
    if (!tokenizer) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    tokenizer->root = createTrieNode();
    tokenizer->num_tokens = 0;
    return tokenizer;
}

int parse_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_python_literal(const char* literal, unsigned char* out, int out_len) {
    int len = 0;
    bool is_byte_string = false;
    
    if (literal[0] == 'b') {
        is_byte_string = true;
        literal++;
    }
    
    char quote = literal[0];
    literal++;
    
    while (*literal != quote && len < out_len) {
        if (*literal == '\\') {
            literal++;
            switch (*literal) {
                case 'n': out[len++] = '\n'; break;
                case 'r': out[len++] = '\r'; break;
                case 't': out[len++] = '\t'; break;
                case '\\': out[len++] = '\\'; break;
                case '\'': out[len++] = '\''; break;
                case '\"': out[len++] = '\"'; break;
                case 'x': {
                    int hex1 = parse_hex(literal[1]);
                    int hex2 = parse_hex(literal[2]);
                    if (hex1 != -1 && hex2 != -1) {
                        out[len++] = (hex1 << 4) | hex2;
                        literal += 2;
                    } else {
                        fprintf(stderr, "Invalid \\x escape\n");
                        return -1;
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Unknown escape sequence: \\%c\n", *literal);
                    return -1;
            }
        } else {
            out[len++] = *literal;
        }
        literal++;
    }
    
    return len;
}

void addToken(Tokenizer* tokenizer, const char* token_literal, int id) {
    unsigned char token[MAX_TOKEN_LENGTH];
    int token_length = parse_python_literal(token_literal, token, MAX_TOKEN_LENGTH);
    
    if (token_length < 0) {
        fprintf(stderr, "Failed to parse token: %s\n", token_literal);
        return;
    }
    
    insertTrie(tokenizer->root, token, token_length, id);
    tokenizer->idx2token[id] = (unsigned char*)malloc(token_length + 1);
    if (!tokenizer->idx2token[id]) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    memcpy(tokenizer->idx2token[id], token, token_length);
    tokenizer->idx2token[id][token_length] = '\0';
    tokenizer->token2idx[id] = id;
    tokenizer->num_tokens++;
}

int* encode(Tokenizer* tokenizer, const char* text, int* num_encoded) {
    int length = strlen(text);
    int* encoded = (int*)malloc(length * sizeof(int));
    if (!encoded) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    *num_encoded = 0;
    int index = 0;
    while (index < length) {
        int endIndex;
        int id = findLongest(tokenizer->root, (const unsigned char*)text + index, length - index, &endIndex);
        if (endIndex == index || id == -1) {
            encoded[(*num_encoded)++] = (unsigned char)text[index];
            index++;
        } else {
            encoded[(*num_encoded)++] = id;
            index += endIndex;
        }
    }
    return encoded;
}

char* decode(Tokenizer* tokenizer, const int* tokens, int num_tokens) {
    int total_length = 0;
    for (int i = 0; i < num_tokens; i++) {
        int id = tokens[i];
        if (id < 256) {
            total_length += 1;  // Single byte character
        } else if (id < tokenizer->num_tokens && tokenizer->idx2token[id]) {
            total_length += strlen((char*)tokenizer->idx2token[id]);
        } else {
            fprintf(stderr, "Unknown token ID: %d\n", id);
            return NULL;
        }
    }
    char* decoded = (char*)malloc((total_length + 1) * sizeof(char));
    if (!decoded) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    char* ptr = decoded;
    for (int i = 0; i < num_tokens; i++) {
        int id = tokens[i];
        if (id < 256) {
            *ptr++ = (char)id;
        } else if (id < tokenizer->num_tokens && tokenizer->idx2token[id]) {
            int length = strlen((char*)tokenizer->idx2token[id]);
            memcpy(ptr, tokenizer->idx2token[id], length);
            ptr += length;
        }
    }
    *ptr = '\0';
    return decoded;
}

void freeTrieNode(TrieNode* node) {
    if (!node) return;
    for (int i = 0; i < 256; i++) {
        if (node->children[i]) {
            freeTrieNode(node->children[i]);
        }
    }
    free(node);
}

void freeTokenizer(Tokenizer* tokenizer) {
    freeTrieNode(tokenizer->root);
    for (int i = 0; i < tokenizer->num_tokens; i++) {
        free(tokenizer->idx2token[i]);
    }
    free(tokenizer);
}

int main() {
    Tokenizer* tokenizer = createTokenizer();
    
    FILE* file = fopen("rwkv_vocab_v20230424.txt", "r");
    if (!file) {
        fprintf(stderr, "Failed to open vocabulary file\n");
        return 1;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        int id;
        char token[256];
        int length;
        
        line[strcspn(line, "\n")] = 0;
        
        char* id_str = strtok(line, " ");
        char* token_str = strtok(NULL, " ");
        char* length_str = strtok(NULL, " ");
        
        if (id_str && token_str && length_str) {
            id = atoi(id_str);
            length = atoi(length_str);
            
            if (token_str[0] == '\'' || token_str[0] == '\"') {
                memmove(token_str, token_str + 1, strlen(token_str));
                token_str[strlen(token_str) - 1] = '\0';
            }
            
            addToken(tokenizer, token_str, id);
        } else {
            fprintf(stderr, "Invalid line format: %s\n", line);
        }
    }
    fclose(file);
    
    printf("Loaded %d tokens\n", tokenizer->num_tokens);
    
    const char* text = R"(Q: System with two quadratic equations Respected All.
I am unable to find out what's so wrong in the following. Please help me.
It is given that $t$ is a common root of the following two equations given by 
egin{align}
&x^2-bx+d=0    ag{1}\
&ax^2-cx+e=0   ag{2}
\end{align}
where $a,b,c,d,e$ are real numbers. 
Then using cross multiplication technique, we shall get
$$
  rac{t^2}{cd-be}=
                  rac{t}{ad-e}=
                               rac{1}{ab-c}   ag{3}$$
which will give us $$tegin{cases}

rac{cd-be}{ad-e},\

rac{ad-e}{ab-c},\
\pm\sqrt{
         rac{cd-be}{ab-c}}
\end{cases}    ag{I}$$
My problems starts from here. If $t$ satisfies both (1) and (2) then any linear combination of (1) and (2) should be satisfied by $t$. So that by $\lambda  imes (1)+\mu        imes (2)$ we shall have 
$$(\lambda+\mu a)t^2-(\lambda b+\mu c)t+(\lambda d+\mu e)=0     ag{4}$$
where $\lambda, \mu$ are suitable reals for (4) to have real roots.
hence we shall get 
$$t=
    rac{1}{2(\lambda+\mu a)}[(\lambda b+\mu c)\pm \sqrt{(\lambda b+\mu c)^2-4(\lambda+\mu a)(\lambda d+\mu e)}]     ag{II}$$
All three results in (I) and the results in (II) are supposed to be same. Aren't they?
So what if we consider $(a,b,c,d,e)=(3,9,38,14,119)$. Then from (I) we shall get $(7,7,7)$. But when we shall apply (II) then the results are becoming "dirty": if we choose $\lambda=-16,\mu=20$ the results are coming as $(7.057474264\cdots, 7.06\cdots)$
Why is it happening ? Am I making any theoratical mistake?

A: We have two polynomial equations $f_1(x)=f_2(x)=0$ with quadratic polynomials
$f_1(x)=x^2-bx+d$ and $f_2(x)=ax^2-cx+e$, where $a,b,c,d,e$ are the coefficients.
Then 
$$
t=
  rac{ \pm \sqrt{b^2 - 4d} + b}{2}
$$ 
is a common root, if and only if the coefficients satisfy certain poylnomial conditions. 
To see this, just substitute this to the second equation.
We can make a case distinction. Assume that $d=0$. Then $t=b$ is a common root for $b
eq 0$ if $a=
            rac{bc - e}{b^2}$, and $c,e$ arbitrary; and for $b=0$ if $e=0$ and $a,b,c$ arbitrary. 
If $d
eq 0$, $t=
          rac{ \sqrt{b^2 - 4d} + b}{2}$ is a common root if and only if
$$
a=
  rac{\sqrt{b^2 - 4d}\cdot be - \sqrt{b^2 - 4d}\cdot cd - b^2e + bcd + 2de}{2d^2}.
$$
A very similar formula holds for the case $t=-
                                              rac{ \sqrt{b^2 - 4d} + b}{2}$. 
For your example  $(a,b,c,d,e)=(3,9,38,14,119)$ the formula gives $t=7$. We have $\sqrt{b^2-4d}=5$, so that $t=
                                                                                                               rac{ \sqrt{b^2 - 4d} + b}{2}=7$, and the relation between $a,b,c,d,e$ is satisfied.
Edit: For your example $(a,b,c,d,e)=(3,9,38,14,119)$ your equation II gives the solutions $t=7$ and $t=(2\lambda + 17\mu)/(\lambda + 3\mu)$. For $\lambda=-16$ and $\mu=20$ I obtain exactly $t=7$, so no problem. I suppose you have done a computational mistake there.)";
    int num_encoded;
    int* encoded = encode(tokenizer, text, &num_encoded);
    if (encoded) {
        printf("Encoded tokens: ");
        for (int i = 0; i < num_encoded; i++) {
            printf("%d ", encoded[i]);
        }
        printf("\n");
        
        char* decoded = decode(tokenizer, encoded, num_encoded);
        if (decoded) {
            printf("Decoded text: %s\n", decoded);
            free(decoded);
        }
        free(encoded);
    }
    
    freeTokenizer(tokenizer);
    return 0;
}
