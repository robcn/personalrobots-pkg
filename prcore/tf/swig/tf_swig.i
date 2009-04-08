 %module tf_swig
 %{
 /* Includes the header in the wrapper code */
#include "tf/tf_swig.h"
 %}
 
%include "std_string.i"

%exception {
  try {
    $function
  }
  catch (tf::TransformException & ex) {
    PyErr_SetString(PyExc_ValueError,ex.what());
    return NULL;
  }
}


 /* Parse the header file to generate wrappers */
%include "tf/tf_swig.h"
