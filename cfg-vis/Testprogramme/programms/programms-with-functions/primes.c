#include <stdio.h>
#include <math.h>

int is_prime(int n) {
    if (n == 2) return 1;
    if (n == 0 || n == 1 || n % 2 == 0) {
        return 0;
    }
    for (int i = 3; i < sqrt(n) + 1; i += 2) {
        if (n % i == 0) return 0;
    }

    return 1;
}

int calculate_nth_prime_number(int n) {
    int lastPrime = 2;
    while (n - 1 > 0) {
        lastPrime++;
        if (is_prime(lastPrime)) {
            n--;
        }
    }
    return lastPrime;
}

int main() {
      for (int i = 0; i < 10; i++) {
          if (is_prime(i)) {
              printf("%d ist eine Primzahl \n", i);
          }
     }
     printf("%d ist die 5. Primzahl \n", calculate_nth_prime_number(5));

     return 0;
}

