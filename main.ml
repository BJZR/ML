# main.ml - Usa biblioteca importada

import "lib.ml";

const bool DEBUG = true;
const i32 STDOUT = 1;
const i32 STDERR = 2;
fn i64 main() {
    var char saludo[20];
    saludo[0] = 'H';
    saludo[1] = 'o';
    saludo[2] = 'l';
    saludo[3] = 'a';
    saludo[4] = '!';
    saludo[5] = '\n';
    saludo[6] = 0;
    
    # Usar función exportada del módulo
    write(STDOUT, &saludo, 6);
    
    if DEBUG {
        var char debug_msg[10];
        debug_msg[0] = 'D';
        debug_msg[1] = 'E';
        debug_msg[2] = 'B';
        debug_msg[3] = 'U';
        debug_msg[4] = 'G';
        debug_msg[5] = '\n';
        
        write(STDERR, &debug_msg, 6);
    }
    
    ret 0;
}
