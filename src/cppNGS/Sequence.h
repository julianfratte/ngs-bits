#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "cppNGS_global.h"
#include <QByteArray>

///DNA or RNA sequence class
typedef QByteArray Sequence;

//**IMPORTANT**
//Right now it is only a typedef.
//Use it anyway - it might be replaced by a own class in the future!

//Notes for the implementation:
//- move NGSHelper::changeSeq to this class!
//- add PERsim::addNoise

#endif // SEQUENCE_H
