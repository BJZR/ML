# Ejemplo completo con TODAS las características

const i32 MAX_NOMBRE = 20;
const bool DEBUG = true;
const f64 PI = 3.14159;

alias Entero = i32;
alias Byte = u8;
alias Real = f64;

struct Punto {
    i32 x;
    i32 y;
}

struct Persona {
    char nombre[MAX_NOMBRE];
    Entero edad;
    Punto ubicacion;
}

fn i64 write(i32 fd, ptr i8 buf, i64 len) {
    ret syscall(1, fd, buf, len);
}

fn i64 printf_simple(ptr i8 fmt, ...) {
    # Función variadic simple - solo imprime el formato
    write(1, fmt, 20);
    ret 0;
}

fn i32 main(i32 argc, ptr i8 argv) {
    # Structs
    var Punto p;
    p.x = 10;
    p.y = 20;
    
    var Persona juan;
    juan.edad = 25;
    juan.ubicacion.x = 100;
    juan.ubicacion.y = 200;
    juan.nombre[0] = 'J';
    juan.nombre[1] = 'u';
    juan.nombre[2] = 'a';
    juan.nombre[3] = 'n';
    juan.nombre[4] = 0;
    
    # Array de structs
    var Punto puntos[3];
    puntos[0].x = 1;
    puntos[0].y = 2;
    puntos[1].x = 3;
    puntos[1].y = 4;
    puntos[2].x = 5;
    puntos[2].y = 6;
    
    # Usar constantes
    var char buffer[MAX_NOMBRE];
    buffer[0] = 'H';
    buffer[1] = 'o';
    buffer[2] = 'l';
    buffer[3] = 'a';
    buffer[4] = ' ';
    buffer[5] = 'M';
    buffer[6] = 'u';
    buffer[7] = 'n';
    buffer[8] = 'd';
    buffer[9] = 'o';
    buffer[10] = '\n';
    
    write(1, &buffer, 11);
    
    # Usar alias
    var Byte b;
    b = 255;
    
    var Real radio;
    radio = 5.0;
    
    # Bool constante
    if DEBUG {
        var char msg[20];
        msg[0] = 'M';
        msg[1] = 'o';
        msg[2] = 'd';
        msg[3] = 'o';
        msg[4] = ' ';
        msg[5] = 'D';
        msg[6] = 'E';
        msg[7] = 'B';
        msg[8] = 'U';
        msg[9] = 'G';
        msg[10] = '\n';
        write(1, &msg, 11);
    }
    
    # Usar argc/argv
    if argc > 0 {
        var char argc_msg[15];
        argc_msg[0] = 'A';
        argc_msg[1] = 'r';
        argc_msg[2] = 'g';
        argc_msg[3] = 'c';
        argc_msg[4] = ':';
        argc_msg[5] = ' ';
        argc_msg[6] = '0' + argc;
        argc_msg[7] = '\n';
        write(1, &argc_msg, 8);
    }
    
    # Función variadic
    printf_simple("Test variadic\n");
    
    ret 0;
}
