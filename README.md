# MLang
MLang es un compilador de un lenguaje minimalista para linux x86_64
---
## ejemplo hello wordl
```
# esta es la funcion write que permite esccribir de manera arcaica..
fn i64 write(i32 fd, ptr i8 buf, i64 len) {
    ret syscall(1, fd, buf, len);
}

# esta es la funcion de arranque..
fn void main(){
  var prt i8 saludo; # declaracion de la variable..
  saludo = "hola, mundo!"; # assignaccion de la variable..
  write(1,&saludo,12); # llama a la funcion para que muestre el valor de la variable saludo.
}
```
