#ifdef __cplusplus
#  define LINKAGE extern "C"
#else
#  define LINKAGE extern
#endif

LINKAGE int StripTool_main (int, char *[]);


int main (int argc, char *argv[])
{
  return StripTool_main (argc, argv);
}
