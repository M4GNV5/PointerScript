# PointerScript
Wild mixture of C and Javascript

##Example code
```javascript
import "printf", "malloc", "free" from "libc.so.6";

var answer = 6 * 7;
printf("THE answer = %d\n", answer);

var names = malloc(sizeof(var) * 4);
names = (pointer)names; //return values of native functiosn are saved in ints
names[0] = "Linus";
names[1] = "Magnus";
names[2] = "rand";

var libc = "libc.so.6";
import names[2] from libc;

var index = rand();
printf("random name = %s\n", names[index]);

free(names); //for good measure
```
