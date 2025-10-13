Program
{
bool es_par(integer n)
{
    bool resultado = true;
    while (n > 0) 
    {
        resultado = !resultado;
        n = n - 1;
    }
    return resultado;
}

void main()
{
    bool b = es_par(3);
    return;
}
}

