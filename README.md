[![Build Status](https://travis-ci.org/NolanRus/lambda.svg?branch=master)](https://travis-ci.org/NolanRus/lambda)

Пока что это парсер лямбда термов. Примеры:

```
x
x x
x x x
x (x x)
(\x . x) x
x (\x . x)
x x (x x) x
\x y . y (x x)
\x y z . x (y z)
\x . x x (x x)
x x (\x . x (\x . x)) x
(\x . x x) (\y z . z x y) (x x x)
```
