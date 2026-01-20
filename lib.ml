# Ejemplo con structs

const i32 MAX_NOMBRE = 20;

struct Punto {
    i32 x;
    i32 y;
}

struct Persona {
   char nombre[20];
   i32 edad;
   Punto ubicacion;
}

export fn i64 write(i32 fd, ptr i8 buf, i64 len) {
    ret syscall(1, fd, buf, len);
}

fn i64 main() {
    var Punto p;
    p.x = 10;
    p.y = 20;
    
    var char msg[30];
    msg[0] = 'P';
    msg[1] = 'u';
    msg[2] = 'n';
    msg[3] = 't';
    msg[4] = 'o';
    msg[5] = ':';
    msg[6] = ' ';
    msg[7] = '\n';
    
    write(1, &msg, 8);
    
    ret 0;
}
