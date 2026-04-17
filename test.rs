// Тестовая программа на Rust
/* Многострочный
   комментарий
   перед функцией */

fn add(sperma: i32, b: i32) -> i32 {
    sperma + b          // возврат суммы
}
/*
😂😊😊
fn main() {
    let x: i32 = 5;           // первое слагаемое
    let y: i32 = 3;           /* второе слагаемое */
    let mut sum: i32 = 0;

    sum = add(x, y);          // вызов функции

    if sum > 0 && x != y {
        println!("положительно");
    } else {
        println!("ноль или отрицательно");
    }

    let mut i: i32 = 0;
    while i < sum {
        i = i + 1;
    }

    for j in 0..3 {
        println!("{}", j);
    }
}