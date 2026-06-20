/**
 * @brief Default yywrap() implementation: always signal end-of-input.
 * @return 1 to stop scanning, 0 to continue with a new input source.
 */
int yywrap(void)
{
	return 1;
}