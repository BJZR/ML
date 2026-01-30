# MLang
MLang es un compilador de un lenguaje minimalista para linux x86_64
---
## ejemplo hello wordl
```
fn i64 write(i32 fd, ptr i8 buf, i64 len) {
    ret syscall(1, fd, buf, len);
}

fn void main(){
  var prt i8 saludo;
  saludo = "hola, mundo!";
  write(1,&saludo,12);
}
```
