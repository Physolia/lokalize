/* ****************************************************************************
  This file is part of Lokalize

  Copyright (C) 2008-2009 by Nick Shaforostoff <shafff@ukr.net>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor approved
  by the membership of KDE e.V.), which shall act as a proxy 
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

**************************************************************************** */

#include "catalogstring.h"


const char* TagRange::getElementName(InlineElement type)
{
    static const char* inlineElementNames[(int)InlineElementCount]={
    "_unknown",
    "bpt",
    "ept",
    "ph",
    "it",
    //"_NEVERSHOULDBECHOSEN",
    "mrk",
    "g",
    "sub",
    "_NEVERSHOULDBECHOSEN",
    "x",
    "bx",
    "ex"
    };

    return inlineElementNames[(int)type];
}

TagRange TagRange::getPlaceholder() const
{
    TagRange tagRange=*this;
    tagRange.start=-1;
    tagRange.end=-1;
    return tagRange;
}

TagRange::InlineElement TagRange::getElementType(const QByteArray& tag)
{
    int i=TagRange::InlineElementCount;
    while(--i>0)
        if (getElementName(InlineElement(i))==tag)
            break;
    return InlineElement(i);
}



QMap<QString,int> CatalogString::tagIdToIndex() const
{
    QMap<QString,int> result;
    int count=ranges.size();
    for (int i=0;i<count;++i)
        result[ranges.at(i).id]=i;
    return result;
}

QDataStream &operator<<(QDataStream &out, const TagRange &t)
{
    return out<<int(t.type)<<t.start<<t.end<<t.id;

}
QDataStream &operator>>(QDataStream &in, TagRange &t)
{
    int type;
    in>>type>>t.start>>t.end>>t.id;
    t.type=TagRange::InlineElement(type);
    return in;
}

QDataStream &operator<<(QDataStream &out, const CatalogString &myObj)
{
    return out<<myObj.string<<myObj.ranges;
}
QDataStream &operator>>(QDataStream &in, CatalogString &myObj)
{
    return in>>myObj.string>>myObj.ranges;
}