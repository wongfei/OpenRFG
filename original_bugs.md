# Original bugs in rfg.exe

Bugs of the original game that the docs of this project record. Entries were compared with the `rfg.exe` disassembly or decompile in Ghidra and with the port, and have a comment at their location in the code. Exceptions say so in a note or are in the sections "Not settled" and "Not in src". Nothing here was run: the comparison is by reading code. Bugs that the port changes on purpose are in [deviations.md](deviations.md). Paths are relative to the repository, `rfg.exe` addresses are virtual addresses.

| Result | Entries |
|---|---|
| Bug in rfg.exe, reproduced by the port | 933 |
| Listed in a doc, but rfg.exe does not behave that way | 11 |
| Code not in `src` | 1 |
| Could not be settled | 3 |
| Comment in the code only, not listed in a doc, not checked individually | 509 |

## Bugs by module

Bugs in `rfg.exe` that the port reproduces.

### Havok SDK

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.cpp:196` | `hkString::memCmp` | `0x00CFF1F0` | hkString::memCmp answers -1 or 1, never the byte difference |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.cpp:123` | `hkStringPtr::printf` | `0x00CF0D20` | hkStringPtr::printf assigns its formatted result twice |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkcachedhashmap.cxx:266` | `hkCachedHashMap::isOk` | `0x00D13E50` | hkCachedHashMap::isOk loops forever on a broken run because the SDK asserts are compiled out |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/tree/hktree.inl:79` | `hkTree::remove` | `0x00F70AE0` | hkTree::remove also frees the removed node's following siblings |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.cpp:21` | `hkLargeBlockAllocator::FixedMemoryBlockServer::bufFree` | `0x00CF6250` | FixedMemoryBlockServer::bufFree clears m_break to null instead of resetting it to the start, so the fixed block cannot be handed out again |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.cpp:23` | `hkLargeBlockAllocator::getMemorySoftLimit` | `0x00CF6AD0` | hkLargeBlockAllocator::getMemorySoftLimit returns 1 instead of a limit |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.cpp:82` | `readInteger64` | `0x00FBF960` | readInteger64 takes 'B'-'F' and 'b'-'f' but not 'A' or 'a' as hex digits |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.cpp:185` | `hkIstream::operator>>(hkBool&)` | `0x00FBFD90` | hkIstream reads "false" by comparing only its first 4 characters |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.cpp:280` | `hkIstream::operator>>(hkStringBuf&)` | `0x00FC0120` | operator>>(hkStringBuf&) appends after the terminator clear() left, so the word reads empty |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/array/hkarraystreamwriter.cpp:80` | `hkArrayStreamWriter::~hkArrayStreamWriter` | `0x00CE8D30` | hkArrayStreamWriter's destructor frees a taken array's 12-byte header only, so its data leaks |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/patches/660/hkpatches_660.cpp:85` | `hkxVertexBuffer_0_to_1` | `0x00F9C930` | hkxVertexBuffer_0_to_1 tries the unsuffixed color name by testing the texture coordinate counter |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/patches/660/hkpatches_660.cpp:117` | `hkxVertexBuffer_0_to_1` | `0x00F9C930` | hkxVertexBuffer_0_to_1 grows the float data by four reals per vertex but writes two |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.cpp:673` | `hkgpConvexHull::expandByPlanarMargin` | `0x00FD5E90` | expandByPlanarMargin expands a point (dimension 0 hull) by half the margin instead of the margin |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.cpp:272` | `hkgpConvexHullImpl::build` | `0x00FF9B40` | Hull's segment degeneracy test compares only x and y of the integer extremes, ignoring z |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkdeepcopier.cpp:156` | `hkDeepCopier::freeDeepCopy` | `0x00F7AF00` | hkDeepCopier::freeDeepCopy reads the trailer at the allocator's reported size, not the size written |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.cpp:80` | `hkObjectCopier::writeZero` | `0x00F7BB70` | writeZero reports TYPE_CSTRING, TYPE_ZERO and TYPE_RELARRAY as unknown and writes nothing for them |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.cpp:212` | `applyDefaultsFor` | `0x00F7B980` | applyDefaultsFor writes a struct member's default and then its members' defaults after it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.cpp:433` | `hkObjectCopier::saveBody` | `0x00F7BE60` | The flags copy ors a failed name lookup's stale value into the leftover bits |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:551` | `hkDataClassDict::removeMember` | `0x00F4CE70` | hkDataClassDict::removeMember shifts from index -1 for a name the class does not declare |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:551` | `hkDataClassDict::removeMember` | `0x00F4CE70` | removeMember with a name the class does not declare shifts from index -1, removing an entry before the array |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:562` | `hkDataClassDict::renameMember` | `0x00F493E0` | hkDataClassDict::renameMember writes before the array (index -1) for an unknown name |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:562` | `hkDataClassDict::renameMember` | `0x00F493E0` | renameMember with an unknown name writes to index -1, before the member array |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:569` | `hkDataClassDict::setMemberDefault` | `0x00F49420` | hkDataClassDict::setMemberDefault writes before the array (index -1) for an unknown name |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:569` | `hkDataClassDict::setMemberDefault` | `0x00F49420` | setMemberDefault with an unknown name writes to index -1, before the member array |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:747` | `hkDataObjectDict::getMemberValue` | `0x00F496B0` | hkDataObjectDict::getMemberValue hands out the member name as the handle instead of its MemberInfo |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:747` | `hkDataObjectDict::getMemberValue` | `0x00F496B0` | getMemberValue hands out the member's name as the handle instead of its MemberInfo |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:953` | `hkDataObjectDict::assign` | `0x00F50780` | hkDataObjectDict::assign(Value) shares a string instead of copying it, so both objects free it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:953` | `hkDataObjectDict::assign` | `0x00F50780` | assign from a Value copies a string's pointer rather than the string, so both objects share and free it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:994` | `hkDataObjectDict::assign` | `0x00F4D3C0` | hkDataObjectDict::assign(array) keeps the reference it took when the array does not match |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:1488` | `ArrayOfTuplesImplementation::View::clear` | `0x00F46800` | ArrayOfTuplesImplementation::View::clear clears the whole flat array instead of the one tuple |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:1974` | `ArrayArrayImplementation::clear` | `0x00F4E020` | ArrayArrayImplementation::clear releases the elements but keeps the size, leaving dangling pointers |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:2888` | `hkDataWorldDict::newClass` | `0x00F47DD0` | hkDataWorldDict::newClass drops the members' default values |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:2912` | `hkDataWorldDict::newArray` | `0x00F47230` | hkDataWorldDict::newArray returns the array after its temporary wrapper released it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.cpp:133` | `hkDataClassNative::_getAllMemberHandles` | `0x00F580F0` | hkDataClassNative::_getAllMemberHandles uses getMember where getDeclaredMember is meant |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.cpp:836` | `hkDataWorldNative::findClass` | `0x00F57530` | hkDataWorldNative::findClass caches a wrapper for unknown and null names |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.cpp:894` | `hkDataWorldNative::newObject` | `0x00F577A0` | hkDataWorldNative::newObject passes isRefOwned false, so the wrapper does not own the new object |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.cpp:49` | `hkPackfileWriter::setContentsWithRegistry` | `0x00F6A280` | setContentsWithRegistry passes the caller's registry to addObject, so a null registry drops virtual contents with a warning |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/packfile/hkpackfilewriter.cpp:157` | `hkPackfileWriter::notDuplicateMetaData` | `0x00F69930` | notDuplicateMetaData replaces a class with conflicting metadata after reporting the error *Note: decompile shows a return after the error; asm 00F699FB falls through to the replacement* |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:288` | `extractText` | `0x01055320` | extractText dereferences the next node without checking it for null |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:627` | `readClassBody` | `0x01055B50` | The object reader gives an array of enums or flags a size of -1 and reads nothing |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:781` | `readClassBody` | `0x01055B50` | A struct element of an unknown class is skipped and its node leaks |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:819` | `readClassBody` | `0x01055B50` | A homogeneous array's class is left unset for an unexpected node and then dereferenced |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:1014` | `eatMarker` | `0x01054C20` | eatMarker skips as many characters as its marker has without comparing them |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:1055` | `hkXmlObjectReader::base64read` | `0x01054C60` | base64read consumes the character after the data whether or not it is '=' |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.cpp:21` | `SequentialNameFromAddress::nameFromAddress` | `0x01058600` | SequentialNameFromAddress prints a new address with the old map size but stores size + 1 |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.cpp:632` | `writeMember` | `0x01058BB0` | The XML object writer prints enum names as format strings |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp:146` | `Reader::~Reader` | `0x00F6C670` | The binary tagfile reader's destructor leaks forward references still pending |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp:238` | `Reader::readHeader` | `0x00F6BDB0` | The unknown-version warning prints m_tagfileVersion instead of the version read |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.cpp:300` | `Reader::_parseClassDefinition` | `0x00F545A0` | _parseClassDefinition skips one extra token after the closing tag |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.cpp:553` | `Reader::_parseArrayItems` | `0x00F54A60` | _parseArrayItems ignores failed references and reads nothing for non-vec tuples |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.cpp:358` | `hkTypeManager::makeArray` | `0x00F51E30` | hkTypeManager::makeArray leaves m_extra uninitialised |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.cpp:460` | `hkTypeManager::replaceTerminal` | `0x00F527B0` | hkTypeManager::replaceTerminal starts its rebuild at the terminal itself |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.cpp:111` | `hkNativePackfileUtils::load` | `0x00F64B00` | Building an hkNativeResource reads the contents type info's name without a null check |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.cpp:372` | `hkNativePackfileUtils::load` | `0x00F64490` | hkNativePackfileUtils::load gives the tracked objects array a byte count as its capacity |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hknativepackfileutils.cpp:402` | `hkNativePackfileUtils::loadInPlace` | `0x00F64820` | loadInPlace returns packfileData - 1 when no section is the contents section |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.cpp:194` | `hkFloatParseUtil::parseFloat` | `0x00F73250` | parseFloat reaches the null TYPE_FINITE entry for an unknown "#" spelling |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmllexanalyzer.cpp:32` | `hkXmlLexAnalyzer::_lexComment` | `0x00F7A780` | _lexComment restarts its match without rechecking the mismatching character, so "--->" never ends a lexer comment (eatComment does end it) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.cpp:44` | `hkXmlParser::translateEntities` | `0x00F6EF00` | translateEntities matches named entities by prefix, so "&ltx;" reads as '<' |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.cpp:129` | `hkXmlParser::canonicalize` | `0x00F6F040` | canonicalize returns one more than the cut string's length after trimming a trailing space |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.cpp:386` | `hkXmlParser::nextNode` | `0x00F6F970` | nextNode does not check a failed text translation (-1 becomes the text length) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlstreamparser.cpp:276` | `hkXmlStreamParser::parseInt` | `0x00F71510` | hkXmlStreamParser::parseInt (both overloads) does not check the length against its 32-byte buffer |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.cpp:80` | `walkDependencies` | `0x00F3E810` | walkDependencies appends a cyclic index to the pending list without counting it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.cpp:192` | `hkpCapsuleTriangleAgent::processCollision` | `0x00DBDB20` | hkpCapsuleTriangleAgent::processCollision sets m_isShortestPoint on every point except the closest |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistutils.h:79` | `hkpMapPointsToSubShapeContactMgr::addContactPointImpl` | `0x00DB5AB0` | The sub-shape contact manager shares one child shape buffer between both bodies |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.cpp:79` | `hkpMultiSphereAgent::processCollision` | `0x00DC8380` | The multi-sphere agent passes the descending loop index as the child's shape key |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.cpp:137` | `hkpMultiSphereAgent::linearCast` | `0x00DC8680` | hkpMultiSphereAgent::linearCast casts every sphere with the first child agent |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h:50` | `hkpProcessCollisionOutput::reserveContactPoints` | `0x00D90CD0` | Neither the output's 256 contact points nor the 256 potential contacts are bounds-checked |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.cpp:138` | `hkCapsuleTriangleAgent3::process` | `0x00DBBD70` | hkCapsuleTriangleAgent3::process sets m_isShortestPoint on every point except the closest |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.cpp:75` | `hkCollectionBvTreeAgent3_extractCachedAabbsOrRecalculate` | `0x00DB7E10` | Collection-collection gather sweeps the child box in place, so the final clamp compares the swept box with itself |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.cpp:147` | `hkConvexListAgent3::cleanup` | `0x00DB4FD0` | The convex list agent3's cleanup runs the predictive GSK cleanup in stream mode too, reading counter and track as a manifold |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx:137` | `hkAgent1nMachine_Process` | `0x00DCD0D0` | 1n process without new keys uses child key 0xFFFFFFFF for every entry since the key is only set from the new keys |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx:137` | `hkAgent1nMachine_Process` | `0x00DCD0D0` | 1n machine process without new keys leaves the child key at 0xFFFFFFFF, so every entry uses the child of the invalid key |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx:456` | `hkAgent1nMachine_Process` | `0x00DCD0D0` | Flipped TIM entry uses minSeparation * 0.5 instead of the unflipped path's 0.1 factor |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx:456` | `hkAgent1nMachine_Process` | `0x00DCD0D0` | The flipped TIM entry without a separating normal uses half the minimum separation where the unflipped entry uses 0.1 of it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.cpp:20` | `hkp3AxisSweep::endOverlapCheckMarker` | `0x00D84CF0` | endOverlapCheckMarker removes index -1 of a marker's overlap list for an object it does not hold, so the size goes negative |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.cpp:264` | `hkp3AxisSweep::hkp3AxisSweep` | `0x00D86A40` | Markers have no y/z endpoints; their indices are the world node's sentinels, so moving a marker node writes into the sentinels |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.cpp:193` | `hkCollideCapsuleUtilCapsVsTri` | `0x00DD2BC0` | hkCollideCapsuleUtilCapsVsTri's manifold search keeps only the deepest first point; the second and third points are those of the last edge |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:238` | `hkpWorld::hkpWorld` | `0x00DE7220` | hkpWorld ctor stores the convex snap threshold in both the convex and the concave member |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:416` | `hkpWorld::hkpWorld` | `0x00DE7220` | An unknown simulation type leaves m_simulation unset and the following write goes through it |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:616` | `hkpWorld::removeAll` | `0x00DE5D60` | removeAll passes the phantom list itself to removePhantomBatch, which reorders it while removing |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:857` | `hkpWorld::getCinfo` | `0x00DE2530` | getCinfo writes the concave snap threshold over the convex field; the cinfo's concave field stays unchanged |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:1469` | `hkpWorld::updateCollisionFilterOnWorld` | `0x00DE68A0` | updateCollisionFilterOnWorld's entity-entity mode empties its removal list per track, so the narrowphase track's rejected agents are never removed |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.cpp:1812` | `hkpWorld::removeActionListener` | `0x00DE1630` | Removing a world listener that is not registered clears the slot before the listener array (index -1) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/hkpsimulation.cpp:91` | `hkSimulation_incrementDeactivationFlags` | `0x00DF5BB0` | hkpSolverInfo::incrementDeactivationFlags xors select flag 0 with both 1 and 2 instead of using flag 1 for the second |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.cpp:523` | `hk4dGskCalcToiFinal` | `0x00E856D0` | Welding against a triangle A drops the welded point; only the normal is kept |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.cpp:608` | `hk4dGskCalcToiFinal` | `0x00E856D0` | TOI contact properties get a stray 321.0 in m_internalDataA |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.cpp:610` | `hk4dGskCalcToiFinal` | `0x00E856D0` | TOI's GSK cache shares its stack slot with the last bisection distance, so unused cache bytes are bytes of that float |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp:549` | `hkCalcMultiPenetrationDepth` | `0x00E894A0` | In multi-penetration depth a failed tetrahedron decrements the shape count, so the next shape is never tried |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp:549` | `hkCalcMultiPenetrationDepth` | `0x00E894A0` | A failed tetrahedron in the multi penetration search lowers the pair count without shifting the order, so the following pair is never tried |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp:551` | `hkCalcMultiPenetrationDepth` | `0x00E894A0` | In multi-penetration depth the expansion check is inverted, so polytopes only grow when their vertex arrays are full |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp:551` | `hkCalcMultiPenetrationDepth` | `0x00E894A0` | The multi penetration free vertex check is inverted, so the polytopes only grow when their vertex arrays are full |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/solve/hkpsolver.cpp:356` | `solve1dLinLimits` | `0x00EAB780` | solve1dLinLimits' max test compares with the accumulated impulse from before the min impulse was applied |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.cpp:299` | `hkpInertiaTensorComputer::computeShapeVolumeMassProperties` | `0x00F1EB00` | computeShapeVolumeMassProperties builds a local identity whose translation w is 0 instead of 1 |

### anim_lib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/anim_lib/code/anim_lib.cpp:1285` | `animlib_get_bone_rel_translation` | `0x00C92650` | animlib_get_bone_rel_translation's index check uses \|\| so it never rejects an index |

### audio_lib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/audio_lib/code/audiolib.cpp:1906` | `audiolib_cue_play` | `0x0049F120` | audiolib_cue_play sets up a merged play again with the new cue's foley data and params |
| `src/volition/audio_lib/code/audiolib.cpp:1911` | `audiolib_cue_play` | `0x0049F120` | audiolib_cue_play wraps the start offset into the variation only in its local params copy |
| `src/volition/audio_lib/code/audiolib_att_loop_rel.cpp:699` | `audiolib_att_loop_rel_start` | `0x004A0680` | The by-name ALR start copies the table params again for a 3D ALR with a position, undoing skip_attack_sound |
| `src/volition/audio_lib/code/audiolib_music.cpp:527` | `audiolib_internal_parse_music_table` | `0x0048FED0` | A music lull name without a colon falls back to the clip element's text instead of its own |
| `src/volition/audio_lib/code/audiolib_vehicle_engine.cpp:205` | `audiolib_engine_range_index` | `0x00497320` | An unknown engine range sound name gives index -1 and its range is written before the array |
| `src/volition/audio_lib/code/stub/audiolib_stub.cpp:519` | `audiolib_hw_unload_sound_bank` | `0x0048AB80` | audiolib_hw_unload_sound_bank writes pData through the streaming wave block it has just freed |

### interface_lib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/interface_lib/code/objects/animation/vint_object_tween.cpp:197` | `interp_ease_out_cubic` | `0x0045C400` | Ease out cubic misses Penner's +1, so it runs from start minus delta to start |
| `src/volition/interface_lib/code/objects/animation/vint_object_tween.cpp:287` | `interp_ease_in_out_sin` | `0x0045C7A0` | Ease in-out sine has Penner's negative sign lost, so it runs from start to start minus delta |
| `src/volition/interface_lib/code/objects/animation/vint_object_tween.cpp:321` | `interp_ease_in_out_circ` | `0x0045C890` | Ease in-out circ adds 2 in its second half, so the root is of a negative number and the result is NaN |
| `src/volition/interface_lib/code/objects/animation/vint_object_tween.cpp:332` | `interp_ease_out_bounce` | `0x0045C960` | Ease out bounce's 0.75, 0.9375 and 0.984375 heights are not scaled by delta |
| `src/volition/interface_lib/code/objects/animation/vint_object_tween.cpp:741` | `vint_object_tween::process_internal` | `0x00476A80` | A cycling tween fires its start event every frame |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp:970` | `vint_element_text::set_insert_values` | `0x004753E0` | set_insert_values reports failure although it set the value *Note: The comment 'Original: reports failure although the value was set' is at both return statements in this function.* |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp:1159` | `vint_element_text::set_leading` | `0x00468ED0` | Text set_leading wraps a leading of 128 to -128 in the signed char field |
| `src/volition/interface_lib/code/scripting/vint_lua_actions.cpp:562` | `vint_debug_sleep` | `0x0046A0C0` | vint_debug_sleep never waits, because its wait is start minus the negated milliseconds wrongly signed |
| `src/volition/interface_lib/code/scripting/vint_lua_actions.cpp:1404` | `vint_get_anim_duration` | `0x0047C0B0` | vint_get_anim_duration fetches the tween duration into its float result rather than the variant added, overwriting the stack |
| `src/volition/interface_lib/code/vint_data_group.cpp:123` | `vint_sub_adopt_pending` | `0x0046B540` | vint_data_item::create_new writes adopted pending subscriptions' handle into the group handle field |
| `src/volition/interface_lib/code/vint_data_group.cpp:186` | `vint_sub_remove_first` | `0x004629E0` | vint_data_group::remove_subscription only ever matches the first subscription and spins forever for any other Also: vint_data_item::remove_subscription only ever matches the first subscription and spins forever for any other |
| `src/volition/interface_lib/code/vint_data_group.cpp:186` | `vint_sub_remove_first` | `0x004629E0` | remove_subscription never moves past the first subscription, so removing any other spins forever |
| `src/volition/interface_lib/code/vint_data_group.cpp:240` | `vint_data_set_template` | `0x0046B6E0` | vint_data_item::set_template tests the element count against 127 instead of the 32 elements Also: vint_data_event::set_template tests the element count against 3 instead of the 8 elements |
| `src/volition/interface_lib/code/vint_data_group.cpp:314` | `vint_data_group::destroy` | `0x004625C0` | vint_data_group::destroy makes each item the head of the removal list, reversing their order |
| `src/volition/interface_lib/code/vint_data_group.cpp:1170` | `vint_data_event::set_element` | `0x00475EA0` | vint_data_event::set_element bounds the index by 32 although an event has 8 elements |
| `src/volition/interface_lib/code/vint_data_group.cpp:1213` | `vint_data_event::get_element` | `0x00463A20` | vint_data_event::get_element bounds the index by 32 although an event has 8 elements |
| `src/volition/interface_lib/code/vint_data_responder.cpp:226` | `vint_data_responder::set_template` | `0x0046BBF0` | vint_data_responder::set_template tests the element count against 127 instead of the 16 elements |
| `src/volition/interface_lib/code/vint_document.cpp:1270` | `vint_document::clip_pop` | `0x0046C250` | clip_pop sets the popped region again rather than the enclosing one |
| `src/volition/interface_lib/code/vint_document.cpp:1688` | `vint_document::parse_xml_resolution_property_set` | `0x004811C0` | The XML color parse never advances through a non-empty value and loops forever |
| `src/volition/interface_lib/code/vint_document.cpp:1688` | `vint_document::parse_xml_resolution_property_set` | `0x004811C0` | The XML color property parse never advances through a non-empty value, so it loops forever |

### keen/gameframework

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:262` | `keen::GameFramework::processInputEvent` | `0x00C0E410` | processInputEvent's Escape and Alt+Enter checks compare WM_CHAR characters in Key events against keen key ids |

### keen/gamelink

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:385` | `kaiko::gamelink::GameLinkLan::update` | `0x00CD8700` | A join times out after timeoutLobbyMember; timeoutJoin is never read |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:443` | `kaiko::gamelink::GameLinkLan::update` | `0x00CD8700` | A lobby state update for more than 12 members exceeds the receivers' 1024-byte buffer |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:519` | `kaiko::gamelink::GameLinkLan::update` | `0x00CD8700` | GameLinkLan never refreshes the lobby info request timer, so a running search broadcasts every update after the first interval |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:550` | `kaiko::gamelink::GameLinkLan::update` | `0x00CD8700` | GameLinkLan::update passes the search results buffer's byte size (800) as its capacity instead of 10 |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:726` | `kaiko::gamelink::GameLinkLan::setLobbyFilters` | `0x00CD35D0` | GameLinkLan::setLobbyFilters always returns false, even when the filters were set |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:781` | `kaiko::gamelink::GameLinkLan::sendP2PPacketUnreliable` | `0x00CD3710` | A station without an address (0.0.0.0:0) counts as sent |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:998` | `kaiko::gamelink::GameLinkLan::handlePacket` | `0x00CD71B0` | The user-defined packet queue is pushed without a capacity check, so a full queue overwrites its oldest packet |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1138` | `kaiko::gamelink::GameLinkLan::handlePacket` | `0x00CD71B0` | A 'lobby not public' join response is reported as JoinLobbyFailed_Locked |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:468` | `GameLinkInternet::update` | `0x00CD7C30` | find-and-join passes the search results array's size in bytes (800) as the result capacity instead of 10 |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:468` | `GameLinkInternet::update` | `0x00CD7C30` | Find-and-join passes the array's byte size (800) as the search results capacity instead of 10 |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:921` | `GameLinkInternet::getNextP2PPacket` | `0x00CD5DE0` | The voice decompression retry passes 4 as the new buffer's size, so it always fails |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:921` | `GameLinkInternet::getNextP2PPacket` | `0x00CD5DE0` | The DecompressVoice retry passes 4 as the new buffer's size, so it always fails |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:1293` | `GameLinkInternet::updateLobbyMembersShared` | `0x00CD61A0` | The member-list update leaks the copy of the old members when the new list overflows |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:1293` | `GameLinkInternet::updateLobbyMembersShared` | `0x00CD61A0` | When the member list capacity is too small, the copy of the old members is leaked |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:1811` | `GameLinkInternet::handleReceivedAuthTicket` | `0x00CD5090` | A received auth ticket's size comes from the packet unchecked and overruns the 1024-byte ticket data |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:1811` | `GameLinkInternet::handleReceivedAuthTicket` | `0x00CD5090` | A received auth ticket's size comes from the packet unchecked and can overflow the ticket data array |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:2280` | `GameLinkInternet::createVoiceContext` | `0x00CD30B0` | createVoiceContext leaks the context and its event on its failure paths |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:2280` | `GameLinkInternet::createVoiceContext` | `0x00CD30B0` | createVoiceContext's early failure returns leak the allocated context and its event |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:2310` | `GameLinkInternet::createVoiceContext` | `0x00CD30B0` | When CreateSourceVoice fails, the context is freed without its destructor, leaking its event |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:2345` | `GameLinkInternet::updateSourceVoiceArray` | `0x00CD5460` | updateSourceVoiceArray skips the element that moves into the slot of an erased voice context |

### keen/input

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/library/modules/runtime/input/sources/global/inputsystem.cpp:250` | `keen::input::sendResetEvents` | `0x00C7AFC0` | sendResetEvents(0xFF) on a focus change reads controllerInfos[255] past the table |

### keen/sound

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:145` | `keen::SoundSystem::freeSound` | `0x00C33080` | freeSound increments the whole run count and flags word, so a run count overflow carries into the flag bits |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:355` | `keen::SoundSystem::initializeBase` | `0x00C39740` | initializeBase reads 30 bus definitions regardless of busCount, past the caller's array |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:821` | `keen::SoundSystem::setVolume` | `0x00C38080` | setVolume with no free automation sets the volume only when another automation runs and the first is not a fade-out |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:866` | `keen::SoundSystem::setPanning` | `0x00C38200` | setPanning has no free-sound check and reads a null definition through a stale matching handle |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:1031` | `keen::SoundSystem::getDataFromSound` | `0x00C360D0` | A single-file sound definition skips the null file check of the other paths |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:1261` | `keen::SoundSystem::createDynamicSoundDefinition` | `0x00C36700` | createDynamicSoundDefinition also takes a definition holding a file whose in-use flag is clear |
| `src/library/modules/runtime/sound/sources/global/soundsystem.cpp:1435` | `keen::SoundSystem::setBusEffectChain` | `0x00C36A50` | setBusEffectChain exits the program when a later call passes a changed effect chain |
| `src/library/modules/runtime/sound/sources/xaudio2/soundsystem_xaudio2.cpp:5` | `keen::SoundSystem::updateDeviceSelection` | `0x00C39410` | The default device check runs on every update after the first second because its timer is never reset |
| `src/library/modules/runtime/sound/sources/xaudio2/soundsystem_xaudio2.cpp:435` | `keen::SoundSystem::createPlatformEffectChain` | `0x00C341B0` | The reverb send matrix puts 0.5 at index 3, so on stereo the right channel is not sent |
| `src/library/modules/runtime/sound/sources/xaudio2/soundsystem_xaudio2.cpp:444` | `keen::SoundSystem::createPlatformEffectChain` | `0x00C341B0` | The reverb XAPO reference is stored and never released |

### render_lib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/render_lib/code/rl_draw.cpp:690` | `rl_draw::string_3d` | `0x005534B0` | rl_draw::string_3d takes glyph widths from the default font (-1), not fontnum |
| `src/volition/render_lib/code/rl_fog_volume.cpp:73` | `rl_fog_volume::system_init` | `0x0053AB50` | The fog volume box mesh draws 37 indices of the 36 in its index buffer |
| `src/volition/render_lib/code/rl_gpu_visibility.cpp:100` | `rl_gpu_vis_system_init` | `0x0053B1F0` | The query box mesh draws 37 indices from the 36 in the index buffer |
| `src/volition/render_lib/code/rl_gpu_visibility.cpp:100` | `rl_gpu_vis_system_init` | `0x0053B1F0` | The GPU visibility box mesh draws 37 indices of the 36 in its index buffer |
| `src/volition/render_lib/code/rl_gpu_visibility.cpp:341` | `rl_gpu_vis_get_state` | `0x0050FAA0` | rl_gpu_vis_get_state draws from the global Vlib_rand_instance on every poll that sees pixels, so the sim's RNG depends on GPU timing |
| `src/volition/render_lib/code/rl_gpu_visibility.cpp:365` | `rl_gpu_vis_get_state` | `0x0050FAA0` | The camera-inside-box path returns VISIBLE through the final loop counter value |
| `src/volition/render_lib/code/rl_gpu_visibility.cpp:391` | `rl_gpu_vis_get_state` | `0x0050FAA0` | m_query_index is decremented by adding 0xFF to the unsigned char |
| `src/volition/render_lib/code/rl_layered_material.cpp:137` | `rl_layered_material::init` | `0x00510820` | rl_layered_material::init never stores the name checksum |
| `src/volition/render_lib/code/rl_layered_material.cpp:137` | `rl_layered_material::init` | `0x00510820` | rl_layered_material::init never stores name_checksum, so m_name_checksum keeps whatever the pool slot held |
| `src/volition/render_lib/code/rl_layered_material.cpp:287` | `rl_layered_material::set_as_render_material` | `0x00526C40` | set_as_render_material does not check the base surface material *Note: The comment is on the line two above; the comment text repeats in cleanup_render_material.* |
| `src/volition/render_lib/code/rl_layered_material.cpp:316` | `rl_layered_material::cleanup_render_material` | `0x00526FE0` | cleanup_render_material does not check the base surface material *Note: The comment is on the line two above.* |
| `src/volition/render_lib/code/rl_material.cpp:175` | `rl_material::hash_internal` | `0x00510FC0` | hash_internal CRCs from the constant_block offset field's address, not the constants it points to |
| `src/volition/render_lib/code/rl_shader_library.cpp:57` | `rl_shader_library::load_shaders` | `0x005462A0` | load_shaders resolves the depth-only shader from "rl_debug_highlight", the highlight shader's name |
| `src/volition/render_lib/code/rl_shader_library.cpp:57` | `rl_shader_library::load_shaders` | `0x005462A0` | The depth-only shader is looked up by the name rl_debug_highlight, the same name as the highlight shader |
| `src/volition/render_lib/code/rl_star_field.cpp:90` | `rl_star_field::initialize` | `0x005191D0` | The star field geometry declares 0xD05 vertices although 6000 are generated |
| `src/volition/render_lib/code/rl_terrain_occluder.cpp:44` | `rl_terrain_occluder::update` | `0x00519830` | The occluder's bottom plane normal m_abc5 is never computed |
| `src/volition/render_lib/code/rl_terrain_occluder.cpp:147` | `rl_terrain_occluder::merge` | `0x004D40F0` | rl_terrain_occluder::merge computes closest points and a direction and drops them, leaving the occluder unchanged |
| `src/volition/render_lib/code/rl_visibility_manager.cpp:185` | `rl_visibility_manager::Debug::drawVisibilityLists` | `0x005349A0` | drawVisibilityLists keeps only its stripped draw's side effect, updating bounding volumes of entities in the debug pass mask |

### rfg

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/game_render_terrain_render.cpp:345` | `load_landmark_lod` | `0x005DACD0` | load_landmark_lod skips a bad header (cpu_offset moves past it) before returning null |
| `src/volition/rfg/code/game_render_terrain_render.cpp:347` | `load_landmark_lod` | `0x005DACD0` | load_landmark_lod does not check rl_material_map::create's result |
| `src/volition/rfg/code/game_render_terrain_render.cpp:448` | `game_render_terrain_landmark_lod_stream` | `0x005DAEB0` | A second landmark with the same name hash is not recorded, so its unstream never releases it |
| `src/volition/rfg/code/multi_server_browser.cpp:64` | `ServerBrowser::startSearch` | `0x005D6E90` | ServerBrowser::startSearch passes an uninitialized query to the filter builder |
| `src/volition/rfg/code/multi_server_browser.cpp:121` | `ServerBrowser::getSearchResults` | `0x005D3AC0` | ServerBrowser::getSearchResults ignores capacity and copies every result |
| `src/volition/rfg/code/multi_server_browser.cpp:138` | `ServerBrowser::startSearch` | `0x005D6E90` | startSearch's game mode filter is one below the published value, so a mode search finds the previous mode |
| `src/volition/rfg/code/nvidia_highlights.cpp:7` | `nvidia_highlights_configure` | `0x005D1510` | The nvidia highlights functions hold the recursive mutex while has_permission locks it a second time *Note: Harmless nesting: the mutex is recursive.* |
| `src/volition/rfg/code/rfg.cpp:545` | `localized_packfile_deinit` | `0x005D2230` | localized_packfile_deinit does not try the non-localized fallback, so a packfile that fell back fails the removal |
| `src/volition/rfg/code/rfg.cpp:1768` | `rfg_get_num_tracked_chunk_ids` | `0x005D2DD0` | rfg_get_num_tracked_chunk_ids returns 12 for an array of three ids |

### rfg/ai

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/ai/ai_building_logic.cpp:120` | `ai_building_do_transition_search` | `0x005DC2E0` | ai_building_do_transition_search is given no box though the caller fetches one *Note: Port drops the caller's unused get_bbox virtual call (0x0064C730).* |
| `src/volition/rfg/code/ai/ai_building_logic.cpp:278` | `ai_building_find_dist_squared` | `0x0064BA40` | ai_building_find_dist_squared reads through a null building |
| `src/volition/rfg/code/ai/ai_building_logic.cpp:332` | `ai_building_is_in_building` | `0x0064BC90` | ai_building_is_in_building reads through a null building |
| `src/volition/rfg/code/ai/ai_building_logic.cpp:389` | `ai_building_collect_substructures` | `0x0064BE20` | ai_building_collect_substructures reads the composite flag from structure, not its root |
| `src/volition/rfg/code/ai/ai_building_logic.cpp:490` | `collect_building_children` | `0x0064C230` | The building collectors walk any child with the mover's sub type without checking its object type |
| `src/volition/rfg/code/ai/ai_building_logic.cpp:606` | `ai_building_find_closest_transition_from_inside` | `0x0064C730` | ai_building_find_closest_transition_from_inside reads through a null structure |
| `src/volition/rfg/code/ai/ai_defense_coordination.cpp:234` | `get_target_info` | `0x00673B30` | get_target_info finds a target and still returns null when the defender is gone |
| `src/volition/rfg/code/ai/ai_scripting.cpp:402` | `ai_scripting_trigget_tech_response` | `0x0064DC80` | ai_scripting_trigget_tech_response only range checks its line and never plays it |
| `src/volition/rfg/code/ai/ai_system.cpp:709` | `ai_system::create_persona` | `0x01D65940` | Ai_mar_on_fire_panic_chance is registered as a tweak but no code reads it *Note: VA is the global; its only xref is the tweak initializer* |
| `src/volition/rfg/code/ai/ambient_action_manager.cpp:728` | `pick_vehicle_to_park` | `0x00650CA0` | pick_vehicle_to_park never compares with its starting 1600, so the last idle driver wins however far away |
| `src/volition/rfg/code/ai/ambient_edf_action_manager.cpp:315` | `pick_flyer_spawn_point` | `0x005DFBC0` | pick_flyer_spawn_point leaves diagonals 5 and 7 unnormalized, so those flyers start about 424 m away, not 300 |
| `src/volition/rfg/code/ai/civilian/ambient_common.cpp:173` | `build_despawn_route_session` | `0x00652D00` | build_despawn_route_session tests broken left and right the opposite way round from build_road_route_session_common |
| `src/volition/rfg/code/ai/civilian/ambient_common.cpp:238` | `build_road_route_session_common` | `0x00653100` | build_road_route_session_common removes a connection past the other road's end from the road's own data, and skips the one swapped in |
| `src/volition/rfg/code/ai/civilian/ambient_common.cpp:663` | `ambient_build_road_waypoints` | `0x00653B40` | ambient_build_road_waypoints drops desired_mode, so every route is built as a despawn route |
| `src/volition/rfg/code/ai/civilian/civilian_common.cpp:192` | `find_last_destination_nav` | `0x005E1040` | find_last_destination_nav starts its best dot at -1 and only takes lower, so it answers null |
| `src/volition/rfg/code/ai/combat/acquire_visual_actions.cpp:269` | `go_to_ally_with_los_action::enter` | `0x006917A0` | go_to_ally_with_los_action::enter adds the cover offset to the old action_pos instead of the node or ally |
| `src/volition/rfg/code/ai/combat/ai_thrown_weapon_logic.cpp:277` | `check_lob_raycast` | `0x006758F0` | check_lob_raycast scales the unit direction by the time to the apex, not the horizontal distance |
| `src/volition/rfg/code/ai/combat/combat_common.cpp:108` | `ai_combat_get_action_firearm` | `0x0061E430` | ai_combat_get_action_firearm returns the unsuitable held weapon when the inventory has no firearm |
| `src/volition/rfg/code/ai/combat/combat_cover_actions.cpp:231` | `find_attack_defensive_cover_node` | `0x00676800` | find_attack_defensive_cover_node reads through a null node when a collected handle no longer resolves |
| `src/volition/rfg/code/ai/combat/combat_cover_actions.cpp:850` | `leave_cover_action::enter` | `0x00676E00` | leave_cover_action::enter passes a navpoint's object handle as the nav cell hint |
| `src/volition/rfg/code/ai/combat/combat_maneuver.cpp:278` | `get_retreat_dir_internal` | `0x005E4B70` | get_retreat_dir_internal weighs by 1 - proportion * 0, reading one past its table, so every direction weighs 1 |
| `src/volition/rfg/code/ai/combat/combat_maneuver.cpp:418` | `do_retreat_vehicle_astar_search` | `0x00658E20` | The retreat A* re-sorts the heap at the cell just removed, not the neighbor that changed |
| `src/volition/rfg/code/ai/combat/combat_maneuver_actions.cpp:96` | `reduce_bbox_size` | `0x005E5E80` | reduce_bbox_size tests the x extent twice, so the z trim is keyed off x and z is effectively never trimmed |
| `src/volition/rfg/code/ai/combat/combat_maneuver_actions.cpp:876` | `advance_action::do_action` | `0x00698090` | advance_action::do_action reads the path's last node with no null check |
| `src/volition/rfg/code/ai/combat/combat_maneuver_actions.cpp:1221` | `fall_back_in_building_action::do_action` | `0x00699110` | fall_back_in_building_action::do_action passes a navpoint handle as the nav cell hint |
| `src/volition/rfg/code/ai/combat/combat_melee_actions.cpp:490` | `move_to_melee_range_action::do_action` | `0x006BC350` | move_to_melee_range_action::do_action compares a squared distance with ai_max_dist |
| `src/volition/rfg/code/ai/combat/combat_thrown_actions.cpp:84` | `get_weapon_from_plan` | `0x00626B70` | get_weapon_from_plan never advances along the plan, so a next action with no weapon hangs |
| `src/volition/rfg/code/ai/combat/cover_selection.cpp:686` | `search_parameters_set_travel` | `0x00678EE0` | cover_search_parameters constructors read initial_target_dist before preprocess sets it, an uninitialized stack float *Note: Inlined at the end of both constructors* |
| `src/volition/rfg/code/ai/combat/cover_selection_internal.cpp:316` | `cover_search_heap::remove_best_node` | `0x005E7040` | remove_best_node leaves the last node with heap index 0, so a removed single node reads as open |
| `src/volition/rfg/code/ai/combat/dodge_actions.cpp:250` | `dodge_sideways_noanim_action::enter` | `0x0065BD70` | dodge_sideways_noanim_action::enter passes a navpoint's object handle as the nav cell hint |
| `src/volition/rfg/code/ai/combat/dodge_actions.cpp:286` | `dodge_run_anim_action::get_cost` | `0x00679AE0` | dodge_run_anim_action::get_cost compares an unnormalized target offset with cos 30 |
| `src/volition/rfg/code/ai/combat/flush_out_actions.cpp:641` | `hold_stairwell_rule::get_cost` | `0x0067A7B0` | hold_stairwell_rule::get_cost takes the selected target for a human with no type test |
| `src/volition/rfg/code/ai/custom_actions.cpp:413` | `sniper_find_new_guard_node` | `0x0065CCC0` | sniper_find_new_guard_node gives its callback a stack local instead of the npc, so it compares the box maximum and excludes handle 0 |
| `src/volition/rfg/code/ai/custom_actions.cpp:522` | `custom_watch_player_action::do_action` | `0x0069CD40` | custom_watch_player_action::do_action reads the player's position after a null test that does not stop it |
| `src/volition/rfg/code/ai/custom_actions.cpp:844` | `custom_follow_wounded::do_action` | `0x0069D2D4` | custom_follow_wounded::do_action tests a member's address for null, which is never null, so a null player crashes |
| `src/volition/rfg/code/ai/follow_actions.cpp:469` | `wait_for_squad_vehicle_action::do_action` | `0x00631A50` | wait_for_squad_vehicle_action treats any vehicle type as an automobile for its brake and steering calls |
| `src/volition/rfg/code/ai/formation_internal.cpp:832` | `ai_formation::add_member` | `0x0065EA00` | ai_formation::add_member leaks its pool entry when the object has no velocity |
| `src/volition/rfg/code/ai/formation_internal.cpp:1020` | `ai_formation::reorder_members` | `0x00632F60` | reorder_members mixes the whole leader_to_follower_dist with the 0.6 scaled distance in its first path index |
| `src/volition/rfg/code/ai/goals.cpp:2974` | `wait_for_passengers_goal::get_fitness` | `0x006A1F30` | wait_for_passengers_goal::get_fitness reads the local player with no null check |
| `src/volition/rfg/code/ai/guard_actions.cpp:105` | `get_best_indoor_navpoint_to_face` | `0x0067EC10` | get_best_indoor_navpoint_to_face reads the root building's flags with no null check |
| `src/volition/rfg/code/ai/investigation_actions.cpp:378` | `play_investigate_voice` | `0x006A4280` | play_investigate_voice compares its random roll with 0, so nearly every roll plays |
| `src/volition/rfg/code/ai/investigation_actions.cpp:729` | `get_disturbance_frustum` | `0x006A45A0` | find_disturbance_action reads the alert level, does nothing with it, and always uses the low frustum |
| `src/volition/rfg/code/ai/investigation_actions.cpp:854` | `examine_disturbance_action::do_action` | `0x006A4740` | examine_disturbance_action ends in failure when the look around animation finishes |
| `src/volition/rfg/code/ai/investigation_actions.cpp:1083` | `examine_corpse_action::do_action` | `0x00662D00` | examine_corpse_action::do_action passes a null killer to human_is_dead when the killer is gone |
| `src/volition/rfg/code/ai/los_queue.cpp:290` | `process_los_request` | `0x006BD3F0` | process_los_request frees the request first and reads its flags from the freed entry afterwards |
| `src/volition/rfg/code/ai/misc_actions.cpp:218` | `get_move_out_of_way_pos_navpoint` | `0x005EBE60` | get_move_out_of_way_pos_navpoint dereferences links on a small navpoint with no null check |
| `src/volition/rfg/code/ai/patrol_actions.cpp:534` | `pick_building_target_pos` | `0x006B7590` | pick_building_target_pos reads terrain height at box-local x and z, before adding the building's position *Note: Inlined twice in patrol_in_vehicle_action::do_action* |
| `src/volition/rfg/code/ai/patrol_actions.cpp:732` | `patrol_in_vehicle_action::do_action` | `0x006B7590` | The blocked patrol takes the driver of its own vehicle rather than the blocking vehicle's driver |
| `src/volition/rfg/code/ai/percept.cpp:747` | `ai_perception::update_or_add_belief` | `0x00664980` | update_or_add_belief(other, relevance, flags, mode) ignores flags and always passes other_belief->flags |
| `src/volition/rfg/code/ai/percept.cpp:1028` | `ai_perception::get_human_belief_flags` | `0x0063D920` | get_human_belief_flags never reads its human, so any acknowledged enemy flags every human |
| `src/volition/rfg/code/ai/percept.cpp:2097` | `ai_perception::update_corpse_belief` | `0x00665E60` | update_corpse_belief gives a corpse beyond 20 m the best relevance and the nearby flag |
| `src/volition/rfg/code/ai/percept.cpp:3081` | `ai_perception::get_melee_move_in_range` | `0x00681570` | get_melee_move_in_range dereferences a null target in its non-human branch Also: The melee pick's non-human target path has no null check before reading the target |
| `src/volition/rfg/code/ai/percept.cpp:4014` | `ai_perception::set_look_at_object` | `0x0063E150` | set_look_at_object dereferences a null object in its non-human branch |
| `src/volition/rfg/code/ai/percept.cpp:4844` | `ai_perception::analyze_shot_result` | `0x0063E630` | analyze_shot_result dereferences target_obj with no null check after the hit tests |
| `src/volition/rfg/code/ai/percept.cpp:7434` | `ai_perception::set_attention_mode` | `0x0063F1E0` | set_attention_mode sets direction to TARGET for acquire visual, then overwrites it with CENTER |
| `src/volition/rfg/code/ai/percept.cpp:7718` | `ai_perception::collect_all_matching_beliefs` | `0x005EFA60` | collect_all_matching_beliefs range check lets type 0x1B through, one past the belief bins |
| `src/volition/rfg/code/ai/plan_conditions.cpp:333` | `ai_plan_conditions_get_for_loaded_firearm` | `0x006401B0` | get_for_loaded_firearm keeps the unsuitable held weapon when the inventory has no firearm, and still reads its reserve |
| `src/volition/rfg/code/ai/search_system.cpp:60` | `get_search_candidate_buildings` | `0x0066B4B0` | get_search_candidate_buildings compares a squared distance with 15, so only buildings within about 3.9 m count |
| `src/volition/rfg/code/ai/search_system.cpp:353` | `ai_search_is_fully_populated` | `0x00685250` | ai_search_is_fully_populated answers true whenever a search target exists, whatever its locations hold |
| `src/volition/rfg/code/ai/search_utils.cpp:207` | `do_next_location_astar_search` | `0x00685A20` | The search A* adjusts the heap at the index of the cell being expanded, not the neighbor that changed |
| `src/volition/rfg/code/ai/self_preservation_actions.cpp:1588` | `flee_to_safe_building_action::do_action` | `0x006AA460` | flee_to_safe_building_action::do_action checks the path against the navpoint but paths to the offset point in its disc |
| `src/volition/rfg/code/ai/self_preservation_actions.cpp:2142` | `flee_action::do_action` | `0x006AA5D0` | flee_action::do_action adds the object's xz size to a squared distance limit |
| `src/volition/rfg/code/ai/self_preservation_actions.cpp:2332` | `flee_in_vehicle_action::do_action` | `0x006AACB0` | flee_in_vehicle_action flees behind the vehicle's facing on enter and behind the npc's velocity afterwards |
| `src/volition/rfg/code/ai/self_preservation_goals.cpp:271` | `ai_get_enemy_flee_belief` | `0x00688800` | ai_get_enemy_flee_belief uses the local player's handle for a bureaucrat with no null check |
| `src/volition/rfg/code/ai/self_preservation_goals.cpp:1260` | `surrender_goal::get_fitness` | `0x005F3310` | surrender_goal::get_fitness always returns -1, so the goal never ranks |
| `src/volition/rfg/code/ai/sorted_list.inl:283` | `sorted_list_base_with_identity::add_unique` | `0x00649CA0` | add_unique does not lower the count when it replaces an element, so a replaced element counts twice |
| `src/volition/rfg/code/ai/threaten_actions.cpp:1454` | `taunt_player_action::get_cost` | `0x005F3C90` | taunt_player_action::get_cost reads the local player's team with no null check |
| `src/volition/rfg/code/ai/vehicle/ai_vehicle_logic.cpp:211` | `get_first_gunner` | `0x00645470` | get_first_gunner calls is_functional on a null turret when a mount is empty or its handle is stale |
| `src/volition/rfg/code/ai/vehicle/ai_vehicle_logic.cpp:1604` | `ai_vehicle_pick_flee_destination_road` | `0x0066F4D0` | ai_vehicle_pick_flee_destination_road leaves the road heap session open when a first query fails |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:1418` | `chase_target_in_vehicle_action::do_action` | `0x006ACBD0` | The chase's nav cell fallback computes a forced point and discards it, leaving the point on the edge |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:1771` | `chase_target_with_fixed_turret::do_action` | `0x006ADF60` | Fixed turret chase uses the chaser's nav cell hint with the target's position |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2019` | `chase_target_with_manned_turret::do_action` | `0x006AE8E0` | Manned turret run in compares a squared distance with the range in metres |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2083` | `manned_turret_pick_side_point` | `0x006AE8E0` | Manned turret's second side try starts from the already moved point, so it tries los_end itself *Note: Inlined in the manned turret do_action* |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2156` | `chase_target_with_manned_turret::do_action` | `0x006AE8E0` | Manned turret compares the xz length of an already normalized vector with half the vehicle's diagonal in metres |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2258` | `chase_target_with_manned_turret::do_action` | `0x006AE8E0` | Manned turret chase uses the chaser's nav cell hint with the target's position |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2464` | `chase_target_with_flying_turret::do_action` | `0x006B04A0` | Flying turret tests the chaser, always a flyer, for the speed boost, so the car boost is never used |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2550` | `chase_target_with_flying_turret::do_action` | `0x006B04A0` | Flying turret collects nearby flyers into a 7 entry stack array with no bound check |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:3371` | `drop_off_passengers_action::do_action` | `0x0068C980` | drop_off_passengers clears its point within sqrt(10) m before driving to it |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:3577` | `acquire_visual_in_vehicle_action::do_action` | `0x006B2B60` | Building corner search with no reachable corner leaves best at FLT_MAX and stops at the nearest corner |

### rfg/camera

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/camera/camera_free.cpp:210` | `unused_dcf_cam_config` | `0x006CABE0` | unused_dcf_cam_config's help and error texts list different arguments and mode names, most of which it does not accept. |
| `src/volition/rfg/code/camera/camera_free.cpp:690` | `do_camera_movement` | `0x006D7180` | do_camera_movement reads through a null target that camera_free_process lets through unless spectating or a dedicated server. |
| `src/volition/rfg/code/camera/camera_free.cpp:1286` | `do_camera_movement` | `0x006D7180` | The chassis follow relaxes only a positive adjust; a negative one stays. The path is dead. *Note: Dead code per the doc: no control type reaches it.* |
| `src/volition/rfg/code/camera/camera_free.cpp:1414` | `camera_free_process` | `0x006D9780` | camera_free_process reads through a null target on the last step, with an early return only for dedicated servers and spectating. |
| `src/volition/rfg/code/camera/camera_free.cpp:1637` | `camera_free_update_swing_to_dir` | `0x006C49F0` | camera_free_update_swing_to_dir uses bash_heading_percent where the swing's own percent looks meant. *Note: Intent is the doc's reading; the binary reads bash_heading_percent.* |
| `src/volition/rfg/code/camera/camera_satellite.cpp:285` | `camera_start_satellite_mode` | `0x006CCB90` | camera_start_satellite_mode with no player leaves the mode pushed and the clock frozen |

### rfg/code_effects

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/code_effects/los_blocker.cpp:83` | `los_blocker_create` | `0x006ED480` | los_blocker_create rotates the offset into the parent's frame but never stores it |
| `src/volition/rfg/code/code_effects/los_blocker.cpp:112` | `los_blocker_do_frame` | `0x006F12A0` | los_blocker_do_frame frees an expired blocker whose parent is also gone a second time |
| `src/volition/rfg/code/code_effects/material_effects.cpp:182` | `unused_dcf_test_matfx_on_player` | `0x006F61A0` | The console test unused_dcf_test_matfx_on_player does not check the character instance is loaded |
| `src/volition/rfg/code/code_effects/material_effects.cpp:225` | `get_ri_tracker` | `0x006F1360` | get_ri_tracker adds the hash entry before the pool null check, leaving a null tracker in the hash |
| `src/volition/rfg/code/code_effects/tracers.cpp:141` | `tracer_alloc` | `0x006EDBE0` | tracer::fvec gets the start position rather than a direction *Note: tracer_alloc is inlined in tracers_create 0x006EDBE0 and curved_tracers_create 0x006EE0E0.* |
| `src/volition/rfg/code/code_effects/tracers.cpp:369` | `expanding_tracer_particle_spread` | `0x006E9AA0` | expanding_tracer_particle_spread does not check the visual is loaded |
| `src/volition/rfg/code/code_effects/tracers.cpp:499` | `tracers_process` | `0x006E9D10` | The curved tracer head covers spline parameter 1 to 2 in its flight time, reaching the end after three flight times |

### rfg/control

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/control/control_map.cpp:809` | `controls_check_swap_confirm_cancel` | `0x006FE760` | The Switch layout takes the alternate button and stick direction from the default table being installed, not the reversed table. |
| `src/volition/rfg/code/control/control_map.cpp:1167` | `controls_poll` | `0x00702230` | controls_poll skips os_received_input only when the first just-released pad button is 0 and the changed axis found is control 0. |
| `src/volition/rfg/code/control/control_map.cpp:1648` | `control_mouse_camera_control` | `0x006FB0B0` | control_mouse_camera_control reads the vertical action's second axis binding instead of its primary one. |
| `src/volition/rfg/code/control/control_map.cpp:2843` | `controls_get_axis_image_name_pc` | `0x00704460` | controls_get_axis_image_name_pc does not check the mouse axis against its 3-entry table. |
| `src/volition/rfg/code/control/control_map.cpp:2938` | `controls_get_button_name_wide` | `0x00704860` | controls_get_button_name_wide converts an unset buffer for a key code of 256 or more. |
| `src/volition/rfg/code/control/control_map.cpp:3006` | `controls_get_axis_name_wide` | `0x00704990` | controls_get_axis_name_wide prints into a buffer that is also an argument. |
| `src/volition/rfg/code/control/control_map.cpp:3041` | `controls_get_mouse_button_axis_name_wide` | `0x006FA050` | The mouse pair name functions read an unset buffer for button numbers of 5 or more. *Note: Comment is on the 5-button check in the wide and narrow versions.* |
| `src/volition/rfg/code/control/control_map.cpp:3113` | `controls_get_gamepad_axis_name_wide` | `0x00702680` | controls_get_gamepad_axis_name_wide prints the whole control value as the axis number. |
| `src/volition/rfg/code/control/control_map.cpp:3140` | `controls_get_axis_name` | `0x00704A40` | controls_get_axis_name sprintfs into a buffer that is also an argument, with no length limit. |
| `src/volition/rfg/code/control/control_map.cpp:3240` | `controls_get_gamepad_axis_name` | `0x007028B0` | controls_get_gamepad_axis_name and _wide read the current config setting and do not use it. *Note: Comment two lines above in both functions; the identical comment text is not a unique anchor.* |
| `src/volition/rfg/code/control/control_map.cpp:3243` | `controls_get_gamepad_axis_name` | `0x007028B0` | controls_get_gamepad_axis_name prints the whole control value as the axis number. |

### rfg/cutscene

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/cutscene/cutscene.cpp:363` | `cs_update_framebuffer_effects_in_shot` | `0x0070BF70` | cs_update_framebuffer_effects_in_shot computes a tween and drops the result |
| `src/volition/rfg/code/cutscene/cutscene.cpp:483` | `cs_initialize_shot_times` | `0x0070C050` | With no camera script a cutscene's total duration is zero |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1054` | `cutscene_stop` | `0x0070C440` | cutscene_stop reads an effect's preloaded and srid fields uninitialized when its handle was -1 |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1283` | `cutscene_start_next_shot` | `0x0070E190` | cutscene_start_next_shot accepts shot_number equal to the count, one past the last shot |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1320` | `cutscene_update_shot` | `0x0070CB80` | cutscene_update_shot accepts shot_number equal to the count, one past the last shot |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1331` | `cutscene_update_shot` | `0x0070CB80` | cutscene_update_shot computes the camera tweens and drops the results |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1493` | `cutscene_do_handshake` | `0x0070CDD0` | cutscene_do_handshake tests a material index against zero rather than -1 |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1525` | `cutscene_get_closest_slate_handle` | `0x0070CF70` | cutscene_get_closest_slate_handle answers the first cutscene slate in the world, wherever it is |
| `src/volition/rfg/code/cutscene/cutscene_audio.cpp:59` | `cutscene_cancel_audio_prime` | `0x00705F00` | Sounds stay paused after a cutscene with no soundtrack, because the unpause is only on the cue path |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:249` | `cs_parse_navpoint` | `0x00706560` | A navpoint's FiddleOrient is not converted from degrees, unlike every other orient in the table |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:302` | `cs_parse_time_of_days` | `0x0070E480` | tod.light_names point into a table that is closed afterwards; nothing reads them |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:662` | `cs_parse_shots` | `0x0070F1F0` | Action effects count towards the shot's effect array size but only CSEffects are read into it |
| `src/volition/rfg/code/cutscene/cutscene_options.cpp:234` | `cutscene_parse_shot_options` | `0x0070D790` | One stack option is reused across cutscene_parse_shot_options, so fields a type does not read carry over |

### rfg/effects

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/effects/object_decal.cpp:1011` | `havok_blast_decal_collector::addCdPoint` | `0x0074D990` | The blast decal collector looks up a collected point's material with the new point's shape key |
| `src/volition/rfg/code/effects/object_decal.cpp:1086` | `blast_decal_collect_points` | `0x0074DCE0` | The blast decal query holds no Havok_raycast_cs and sets no object filter |
| `src/volition/rfg/code/effects/object_decal.cpp:1156` | `object_decal_create_blast_decals` | `0x0074E340` | The shape ray's hit point, in the body frame and unrotated, is compared with the world hit position |
| `src/volition/rfg/code/effects/object_effect.cpp:547` | `object_effect::stream` | `0x0074D060` | Re-applying a saved stop style on stream-in is a no-op |
| `src/volition/rfg/code/effects/object_effect.cpp:563` | `object_effect::stream` | `0x0074D060` | object_effect::stream reads the district tint through a null district when there is no district or default |
| `src/volition/rfg/code/effects/object_effect.cpp:1383` | `vfx_physics_rigid_body_create_cb` | `0x007468A0` | vfx_physics_rigid_body_create_cb does not check the created debris object |
| `src/volition/rfg/code/effects/object_effect_profile.cpp:37` | `object_effect_profile::init` | `0x0074ACA0` | object_effect_profile::init builds and drops a fresh data block without resetting the statistics |

### rfg/gameplay

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/gameplay/activities/activity.cpp:320` | `select_activity_spawn_object` | `0x007693B0` | select_activity_spawn_object makes a vrand roll whose result goes unused, still advancing the generator |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:449` | `count_ready_activities` | `0x007A8230` | count_ready_activities divides control by a zero maximum when there is no district |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:572` | `do_activity_spawn_frame` | `0x007A83F0` | do_activity_spawn_frame dereferences a null local district when the set came from the activity alone |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:2800` | `activity_squad_destroy_notify` | `0x007B55B0` | activity_squad_destroy_notify keeps walking after it ends a raid, so a squad listed twice ends it twice |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:3041` | `activity_get_player_start` | `0x0077F690` | activity_get_player_start has no limit on the plain player start walk, so more than 10 overrun the array |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:3089` | `activity_warp_and_start` | `0x0077F770` | activity_warp_and_start dereferences a null start object when activity_node is null |
| `src/volition/rfg/code/gameplay/activities/activity.cpp:3103` | `activity_force_start_generic` | `0x007A8710` | activity_force_start_generic does not start riding shotgun, area defense or house arrest nodes, only reaching the warp |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:149` | `load_area_defense_table` | `0x00796240` | Area Defense table: a reload with more rows returns and leaves the table open |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:150` | `load_area_defense_table` | `0x00796240` | Area Defense table: an unknown kill type leaves the field as it was Also: Area Defense table: a row with no Base_Data keeps what the entry held |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:406` | `area_defense_reinforcement_info::reset` | `0x00753E60` | area_defense_reinforcement_info::reset leaves ca_index unset, which nothing reads |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:492` | `activity_area_defense_data::collect_spawn_objects` | `0x0077F9C0` | Area Defense: collect_spawn_objects does not ask can_spawn |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:934` | `activity_area_defense_data::test_reinforcement_conditions` | `0x007B1440` | Area Defense: only the first two changes of wave say a line |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:1030` | `activity_area_defense_data::do_frame_active_wait_player` | `0x00756F30` | Area Defense: the GPS target asked for in do_frame_active_wait_player is dropped |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:1050` | `activity_area_defense_data::do_frame_active_running` | `0x007B1610` | Area Defense: the second half of the flee test fails the activity for a player back inside the cylinder, which cannot be true there |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:192` | `activity_assassin_data::activity_do_frame_ready` | `0x007ACE20` | The assassin's activity_do_frame_ready dereferences a null office object |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:246` | `activity_assassin_data::load_definition_from_table` | `0x0077FC10` | The assassin table loader's early returns leave the common pool unrestored |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:337` | `activity_assassin_data::spawn_bureaucrats` | `0x007968C0` | spawn_bureaucrats has no caller in rfg.exe, so an assassination gets no bureaucrats |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:378` | `activity_assassin_data::spawn_bodyguards` | `0x007B17C0` | spawn_bodyguards has no caller in rfg.exe, so an assassination gets no bodyguards |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:426` | `activity_assassin_data::monitor_bureaucrats` | `0x007B1960` | monitor_bureaucrats steps over the entry after one it removes and dereferences a null npc |
| `src/volition/rfg/code/gameplay/activities/activity_car_bomb.cpp:51` | `activity_car_bomb_data::get_ui_display_info` | `0x00757350` | activity_car_bomb_data::get_ui_display_info answers true with a null buffer |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:306` | `base_continuous_attack_data::load` | `0x00780180` | respawn_period is truncated to whole seconds before it is scaled to milliseconds |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:449` | `activity_continuous_attack_data::activity_do_frame_active_patrol` | `0x00780340` | A squad at the end of its patrol posts the reached-end event each frame until it moves off |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:594` | `activity_continuous_attack_data::do_frame_update_pos` | `0x00780670` | do_frame_update_pos has no caller, so the continuous attack's target position stays where it started |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:676` | `activity_continuous_attack_data::get_squads` | `0x0076A450` | get_squads walks the array's count, which stays zero because slots are used by capacity, so it answers 0 |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:139` | `collect_convoy_objects_random` | `0x007808C0` | Convoy: collect_convoy_objects_random adds every node once with no conditions and the good ones a second time |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:666` | `activity_convoy_data::set_member_flags` | `0x00757810` | Convoy: disable_for_player never reaches the vehicle's flags |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:752` | `activity_convoy_data::set_active_flags_capture` | `0x0076AFEC` | Convoy: set_active_flags_capture clears the target's entry after setting it, which only the asm shows |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:904` | `activity_convoy_data::activity_do_frame_ready` | `0x00780C50` | Convoy: of the three progress lines, only the halfway one plays |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:274` | `activity_courier_chase_data::check_for_start` | `0x007B5AD0` | Courier: starting checks run only for a squad that is not simulated |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:470` | `activity_courier_chase_data::check_for_end` | `0x007B74A0` | Courier: a squad out of the streamed world counts as out of range at once |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:917` | `activity_courier_chase_data::update_destination_ghost` | `0x0076C0F0` | Courier: nothing fills courier_route, so the ghost route code returns at once |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:1134` | `activity_cc_abandon_vehicle` | `0x00757F10` | activity_cc_abandon_vehicle is empty and always answers false |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:498` | `activity_delivery_data::activity_do_frame_active` | `0x007B7D20` | Delivery: the post-death timer is set again each frame while the player is dead, so it never runs out |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:515` | `activity_delivery_data::activity_do_frame_active` | `0x007B7D20` | Delivery: the clock is reset each frame until the vehicle moves |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:561` | `activity_delivery_data::activity_do_frame_active` | `0x007B7D20` | Delivery: the time warning only clears its flag and plays nothing |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:842` | `activity_delivery_data::get_current_time` | `0x0076C830` | Delivery: activity_start marks the end node as running while get_current_time reads the start node |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:909` | `activity_delivery_start_activity` | `0x007A0FB0` | activity_delivery_start_activity starts nothing; it only answers whether it could start |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:225` | `activity_demolitions_master::reset` | `0x00758460` | Demolitions Master: reset leaves player_start_end_position, post_activity_timer and out_of_ammo_timer as they were |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:513` | `activity_demolitions_master::do_loading_process` | `0x007A11C0` | A clean-up child of another object type with the mover sub type passes null to stress_evaluation_stop |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:560` | `activity_demolitions_master::reload_inventory` | `0x00758540` | activity_inventory_save_file keeps a closed file's pointer after reload_inventory |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:720` | `activity_demolitions_master::get_completion_text_part_2` | `0x007586A0` | get_completion_text_part_2 is a blank with data, and the string it was copied into without |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:805` | `activity_demolitions_master::get_salvage_received` | `0x00758720` | get_salvage_received counts a completion, so a run whose screen never asks does not count |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:940` | `activity_demolitions_master::activity_do_frame_active` | `0x007A1430` | The 5 second warning is skipped when the time limit has under 5 seconds left |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:207` | `is_roadblock_child` | `0x0076D2B0` | is_roadblock_child reads through a null object when a handle's object is gone Also: do_spawn_barricade can pass is_roadblock_child a null parent object |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:452` | `activity_house_arrest::check_hostage_execution_timer` | `0x0076D620` | A hostage not yet spawned when the kill timer runs out ends the activity instead |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:453` | `activity_house_arrest::check_hostage_execution_timer` | `0x0076D620` | When hostage_kill_timer runs out, living hostages join the player's team; nothing kills them |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:544` | `activity_house_arrest::activity_do_frame_active` | `0x007B60E0` | NEAR_HOUSE_DISTANCE is written and never read *Note: One xref in rfg.exe (write at 0x007B60FB); the port leaves the dead store out.* |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:585` | `activity_house_arrest::activity_do_frame_active` | `0x007B60E0` | activity_do_frame_active skips reinforcements and the two hostage checks for a frame when no one can say the freed line |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1049` | `activity_house_arrest::play_chat_line` | `0x00782A10` | vehicle_complaint_period is armed once, so after three minutes every chat in a far vehicle complains |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1430` | `activity_house_arrest::record_victory` | `0x007B6240` | The morale reward subtracts the penalty for each hostage not rescued and again for each the player killed |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1472` | `activity_house_arrest::do_frame_edf_count` | `0x0076DFD0` | Nothing rearms defender_count_event, so do_frame_edf_count counts every frame |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1738` | `activity_house_arrest::render_icons` | `0x00783220` | render_icons reads activity_spawn_node_handle after start_activity_spawn cleared it, so the default colour is used |
| `src/volition/rfg/code/gameplay/activities/activity_mayhem.cpp:361` | `activity_mayhem::count_object_damage` | `0x007590A0` | Mayhem's vehicle team test compares the whole spawn flag word with 1 |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:96` | `activity_mission_vrail_data::render_icons` | `0x00783600` | Mission Rail render_icons does not draw the player |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:155` | `activity_mission_vrail_data::activity_end` | `0x007ADEA0` | Mission Rail: activity_end releases the shared data twice |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:174` | `activity_mission_vrail_data::activity_kill` | `0x00798CA0` | Mission Rail: activity_kill asks is_active after clearing it, so it never reaches activity_end and the definition stays |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:232` | `activity_mission_vrail_data::activity_do_frame_active` | `0x007B8160` | Mission Rail: a failed rail runs on until a script cancels it |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:294` | `should_give_edf_attack_orders` | `0x007837F0` | should_give_edf_attack_orders reads the offensive raid's participants even in a defensive raid |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:1571` | `activity_raid_data::do_frame_stat_tracking` | `0x007A22C0` | do_frame_stat_tracking compares defenders_alive with both the attacker and the defender limits |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:2127` | `activity_raid_data::do_frame_referee` | `0x007A2580` | do_frame_referee says the winning line twice when there is no demolition |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:2318` | `activity_raid_data::accept_new_squad` | `0x00784780` | accept_new_squad gives an attacker the defenders' first box and a defender the attackers' first |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:2662` | `activity_raid_data::queue_raid_watch` | `0x00784A60` | queue_raid_watch flags each watcher but replans only the last npc the world walk reached |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:2870` | `activity_raid_get_exit_point` | `0x0076FCF0` | activity_raid_get_exit_point reads element 0 of the exit cell list without checking it has one |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:3265` | `activity_raid_get_target` | `0x00770120` | activity_raid_get_target reads element 0 of the target list without checking it has one |
| `src/volition/rfg/code/gameplay/activities/activity_register_damage.cpp:75` | `register_mark_object` | `0x00770510` | register_mark_object dereferences the result of its second human lookup without a null check |
| `src/volition/rfg/code/gameplay/activities/activity_tanks_a_lot.cpp:229` | `activity_tanks_a_lot::activity_do_frame_ready` | `0x00770A00` | Tanks a Lot kills itself on reaching ready, so no spawn ever runs it |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:89` | `activity_vehicle_rail_data::register_death` | `0x0078600D` | Too_many_civilians_killed is set and never read *Note: Two xrefs in rfg.exe, both writes.* |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:227` | `load_riding_shotgun_table` | `0x00799E60` | Riding shotgun table: a reload with more rows returns and leaves the table open *Note: Comment is the next line, the same text as a second load function.* |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:520` | `activity_vehicle_rail_data::collect_spawn_objects` | `0x00785DD0` | Riding Shotgun: collect_spawn_objects does not ask can_spawn |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:855` | `activity_vehicle_rail_data::register_damage` | `0x0079A240` | Riding Shotgun: a vehicle left at or under 750 hit points scores all it had before the hit |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:927` | `activity_vehicle_rail_data::register_death` | `0x00785EF0` | Riding Shotgun: a non-human kill scores as building_damage, whatever it was |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1308` | `activity_vehicle_rail_data::do_frame_active_wait_player` | `0x00786610` | Riding Shotgun: do_frame_active_wait_player has no return after activity_end, so the rest runs on the released rail |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1386` | `activity_vehicle_rail_data::do_frame_active_running` | `0x007B8440` | Riding Shotgun: a failed rail never ends from do_frame_active_running, because the test asks whether the run is running |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1398` | `activity_vehicle_rail_data::render_icon_player` | `0x00786890` | Riding Shotgun render_icons does not draw the player; it looks the squad up and draws nothing |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1403` | `activity_vehicle_rail_data::render_icon_attackers` | `0x007868B0` | Nothing sets Render_reinforcement_icon, so the attacker icons never draw *Note: All six xrefs in rfg.exe are reads.* |
| `src/volition/rfg/code/gameplay/activities/base_activity_data.cpp:332` | `base_activity_data::load` | `0x007A2870` | ratchet_alert_level is compared case sensitively with True, unlike the other table booleans |
| `src/volition/rfg/code/gameplay/activities/delivery_common.cpp:196` | `base_delivery_definition_data::load` | `0x00786BE0` | Delivery: the radio voice set is rolled once when the table loads, whatever the delivery |
| `src/volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp:200` | `can_use_nav_cell` | `0x0075B0C0` | can_use_nav_cell for a human requires flag 8 set, the opposite of the vehicle test |
| `src/volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp:1237` | `reinforcement_allocation::do_frame_inactive` | `0x007B6F20` | do_frame_inactive releases the first live squad and returns, waiting for the next frame |
| `src/volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp:1799` | `find_spawn_location_road` | `0x00787B10` | A road search stores a connection's point index as connection_dist, later read as a position on the other road |
| `src/volition/rfg/code/gameplay/activities/reinforcement_squad.cpp:283` | `reinforcement_squad::update_target_beliefs` | `0x00771C90` | update_target_beliefs for a squad target uses the leader of the squad itself instead of the target's leader |
| `src/volition/rfg/code/gameplay/activities/reinforcement_squad.cpp:409` | `reinforcement_squad::can_engage_target` | `0x00771F20` | can_engage_target for a squad target uses the leader of the squad itself instead of the target's leader |
| `src/volition/rfg/code/gameplay/activities/vrail_common.cpp:372` | `shared_vrail_patrol_data::do_frame` | `0x0079B610` | shared_vrail_patrol_data::do_frame asks the global queries, not its own rail |
| `src/volition/rfg/code/gameplay/activities/vrail_common.cpp:427` | `shared_vrail_spawn_data::release_and_reset` | `0x0079B7F0` | release_and_reset clears the rail flags on every region and NPC, also those of the other rail |
| `src/volition/rfg/code/gameplay/activities/vrail_data.cpp:123` | `vehicle_rail_definition::load` | `0x0078AA20` | vehicle_rail_definition::load tests the entry, not the tag, for vehicle_group, so a missing tag reads through null |
| `src/volition/rfg/code/gameplay/activities/vrail_system.cpp:110` | `vrail_is_vehicle` | `0x0078AE30` | vrail_is_vehicle only asks Riding Shotgun |
| `src/volition/rfg/code/gameplay/air_bombing.cpp:73` | `unused_dcf_air_bomb_start` | `0x00772760` | unused_dcf_air_bomb_start passes -1 as the origin handle, so air_bomb_start always fails |
| `src/volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp:171` | `base_alert_level_values::load` | `0x0075D050` | A decay rate never falls below 1, because the clamp in load only has a floor |
| `src/volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp:453` | `load_alert_level_ranges` | `0x00772F90` | A missing tag in load_alert_level_ranges returns without closing the table |
| `src/volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp:1106` | `alm_report_vehicle_death` | `0x0079C710` | A vehicle killed by its own side keeps death_reported clear, so the next report comes back through the same path |
| `src/volition/rfg/code/gameplay/gameflow/gfm_control_morale.cpp:538` | `gfm_change_edf_control` | `0x007B00B0` | The EDF control event says lowered whichever way the control moved |
| `src/volition/rfg/code/gameplay/object_district.cpp:807` | `district::change_control` | `0x007ABED0` | district::change_control does nothing outside a load once control is zero Also: district::change_control tries to liberate the district on any change, up or down |
| `src/volition/rfg/code/gameplay/object_district.cpp:1030` | `district::get_loading_screen_images` | `0x0075F210` | get_loading_screen_images only answers the count and never fills image_names |
| `src/volition/rfg/code/gameplay/road_distance_info.cpp:72` | `build_road_info_session` | `0x0078FD30` | build_road_info_session ignores its road argument, overwrites its index and looks the nearest road up again |
| `src/volition/rfg/code/gameplay/safehouse_vip.cpp:201` | `stream_notify_select_spawn` | `0x00790430` | A failed VIP roll records the set's parent with no node, so the set stays decided until it unloads |
| `src/volition/rfg/code/gameplay/upgrades.cpp:1001` | `upgrade_unlock_from_activities` | `0x007764E0` | An upgrade level unlocks from activities only when the completed count equals its number exactly |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp:1281` | `wrecking_crew_change_state` | `0x007A5EA0` | The challenge match winner compares all four player slots regardless of num_players. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp:1394` | `wrecking_crew_do_round_end` | `0x007A68E0` | The challenge round end requests the map restore twice, the second overriding the first. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp:2182` | `wrecking_crew_lock_map` | `0x00777310` | wrecking_crew_lock_map loops forever when every map is locked. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp:2429` | `wrecking_crew_get_player_name` | `0x00777360` | wrecking_crew_get_player_name builds the "player N" tag and never uses it. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.cpp:61` | `wrecking_crew_mode_fall_guy_round_begin` | `0x00790F80` | Fall Guy with fewer than five barrels leaves Fall_guy_important_barrels null or stale and the walks read through it. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.cpp:215` | `wrecking_crew_mode_fall_guy_round_begin` | `0x00790F80` | The Fall Guy barrel filter calls strstr("Default", name) with its arguments reversed. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.cpp:140` | `reset_walker` | `0x0079F3C0` | reset_walker reads a walker that failed to be created through null. |
| `src/volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.cpp:333` | `wrecking_crew_create_walker` | `0x00791720` | wrecking_crew_create_walker reads the handle of a failed creation through null. |

### rfg/gamewide

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/gamewide/game_options.cpp:180` | `game_options_ini_file_read_writer::parse_display_settings` | `0x007E76E0` | The performance mode is always GOPM_HIGH_PERFORMANCE because the PC build enumerates one mode |
| `src/volition/rfg/code/gamewide/game_options.cpp:248` | `game_options_ini_file_read_writer::read_or_write_ini_file` | `0x007EB740` | The ini file reads tank_stick_controls twice |
| `src/volition/rfg/code/gamewide/game_options.cpp:286` | `game_options_ini_file_read_writer::read_or_write_ini_file` | `0x007EB740` | The ini file's camera_shake holds the inverse of the option's value |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:166` | `gameaudio_get_system_init_params` | `0x007BDC2E` | Gameaudio_streaming_file_initialize_result is never set, so audio_lib gets no game file loader |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:582` | `gameaudio_process` | `0x007BB930` | gameaudio_process is registered on the audio thread but does nothing |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:595` | `ga_register_for_global_events` | `0x007BB950` | ga_register_for_global_events registers for no event |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:900` | `gameaudio_parse_settings` | `0x007BBCE0` | The crossover link count is the number of entries, not of the pairs that resolved |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:934` | `gameaudio_parse_settings` | `0x007BBCE0` | The voice chat slider cue is read from the User_Voice_Adjust element |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:1398` | `gameaudio_mp_preload` | `0x007BDED0` | gameaudio_mp_preload is empty in retail *Note: The comment 'Empty in retail.' sits on the line above.* |
| `src/volition/rfg/code/gamewide/gameaudio.cpp:1404` | `gameaudio_mp_unload` | `0x007BDEE0` | gameaudio_mp_unload is empty in retail *Note: The comment 'Empty in retail.' sits on the line above.* |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:220` | `gamemusic_start_special_music` | `0x007CBB30` | gamemusic_start_special_music's inlined copy loop leaves gamemusic_forced_stop false, clearing the forced stop |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:233` | `gamemusic_parse_music_list_init` | `0x007BE080` | gamemusic_parse_music_list_init allocates from audio_lib's pool, not the pool it is given Also: gamemusic_parse_music_list_init returns true when its allocation fails |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:353` | `gamemusic_parse_music_list_item` | `0x007BE180` | The silence level's melodic percentage takes the neutral percentage |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:413` | `gamemusic_parse_music_list_item` | `0x007BE180` | The light lull level reads its times from a Combat_Light_Time element, not a lull element |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:440` | `gamemusic_parse_music_list_item` | `0x007BE180` | The heavy level reads its times only when it has an exit transition |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:980` | `gamemusic_determine_EDF_combat_intensity` | `0x007D8050` | The light vehicle intensity factors take the count of all drivers, not of the light vehicles |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:1106` | `gamemusic_compute_current_intensity_single_player` | `0x007D8310` | The intensity threshold search never selects silence, since ambience's threshold is always 0 *Note: The loop can reach index 0; only the zero threshold of ambience prevents it.* |
| `src/volition/rfg/code/gamewide/gamemusic.cpp:1488` | `multiplayer_music_play_cue` | `0x007BF100` | multiplayer_music_play_cue picks among five tracks for both teams, though the unused selector has six for guerrillas |
| `src/volition/rfg/code/gamewide/stats.cpp:17` | `stats_level_init` | `0x007E5A90` | stats_level_init, stats_set_stat and stats_set_stat_fl check Hidden_challenges[0..n) instead of the stat's own references *Note: also stats_set_stat 0x007E5B10, stats_set_stat_fl 0x007E5C10* |
| `src/volition/rfg/code/gamewide/stats.cpp:265` | `get_weapon_info_from_stat` | `0x007C6EB0` | get_weapon_info_from_stat tests the kill range before the spiker, so the spiker's kill stat never gets a weapon |
| `src/volition/rfg/code/gamewide/stats.cpp:889` | `parse_stats_data` | `0x007D39A0` | parse_stats_data stops reading stats.xtbl at the first unknown stat name, dropping the rest of the table |
| `src/volition/rfg/code/gamewide/stats.cpp:983` | `parse_stats_data` | `0x007D39A0` | Store_in_Save_Game is compared with "True" while the other flags compare with "true" |
| `src/volition/rfg/code/gamewide/stats.cpp:2067` | `stats_get_achievement_progress` | `0x007C7F50` | stats_get_achievement_progress reads Achievements[-1] for an unknown id; its null check can never fail |
| `src/volition/rfg/code/gamewide/stats.cpp:2278` | `stats_serialize_in_profile_stats` | `0x007EA1E0` | stats_serialize_in_profile_stats updates a stat without a set function with a null delta |
| `src/volition/rfg/code/gamewide/stats.cpp:2430` | `stats_serialize_in_save_game_stats` | `0x007C8780` | stats_serialize_in_save_game_stats reads save game bool stats but never sets them |
| `src/volition/rfg/code/gamewide/stats.cpp:2439` | `stats_serialize_in_save_game_stats` | `0x007C8780` | stats_serialize_in_save_game_stats calls set_stat_data_fn without a null check for the types that have none |
| `src/volition/rfg/code/gamewide/stats.cpp:2578` | `stats_count_completed_activities` | `0x007E5DF0` | stats_count_completed_activities reads the demolitions master level data's flag without the null check is_completed has |
| `src/volition/rfg/code/gamewide/verify_gamesave.cpp:6` | `verify_gamesave_process` | `0x007EFFF0` | The save is copied into the current world state buffer before platform_load selects WSM_DEFAULT, so a load after a checkpoint runs from the default buffer |

### rfg/geom

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/geom/cloth_simulation.cpp:15` | `cloth_simulation::pre_simulate` | `0x008028A0` | pre_simulate dereferences the parent player without a null check after a failed handle lookup |
| `src/volition/rfg/code/geom/cloth_simulation.cpp:457` | `cloth_simulation::post_simulate` | `0x008040B0` | post_simulate reads node_infos[-1] for a node without a parent |
| `src/volition/rfg/code/geom/cloth_simulation_shared_spu.cpp:154` | `cloth_simulation::simulate` | `0x00805090` | The cloth wind strength eases by comparing wind_heading_offset with the strength instead of the strength offset |
| `src/volition/rfg/code/geom/cloth_simulation_shared_spu.cpp:200` | `cloth_simulation::simulate` | `0x00805090` | The 150-entry stack velocity array is not checked against the cloth's node count |
| `src/volition/rfg/code/geom/material_shared.cpp:32` | `material_load_rl_data` | `0x00808490` | material_load_rl_data checks neither rl_material::create nor rl_material_map::create |
| `src/volition/rfg/code/geom/rfg_material.cpp:25` | `material_load` | `0x00808AD0` | A material of a container without a srid is loaded but not kept, and still reported as loaded |
| `src/volition/rfg/code/geom/vanim.cpp:2880` | `vanim_process_obj_movements` | `0x007F8200` | vanim_process_obj_movements reads through a null anim on an out-of-range anim handle. |
| `src/volition/rfg/code/geom/vanim.cpp:3292` | `vanim_attach_actions` | `0x007F9B10` | vanim_attach_actions tests hp_2's move_to_starting_orient using hp_1's action index instead of hp_2's. |
| `src/volition/rfg/code/geom/vanim.cpp:3293` | `vanim_attach_actions` | `0x007F9B10` | vanim_attach_actions writes through index_2 without checking it against -1. |
| `src/volition/rfg/code/geom/vanim.cpp:3647` | `execute_prop_trigger` | `0x00800070` | execute_prop_trigger dereferences a null source or destination for unusual owners and world destinations |
| `src/volition/rfg/code/geom/vanim.cpp:3892` | `vanim_check_prop_handles` | `0x00800570` | Blend tree state sound triggers are not muted before multiplayer gameplay or wrecking crew turns |
| `src/volition/rfg/code/geom/vanim.cpp:3925` | `vanim_check_prop_handles` | `0x00800570` | Prop trigger elapsed masks are one byte wide, so a ninth trigger and later run every time *Note: Same in vanim_fast_forward_prop_triggers 0x00800460.* |

### rfg/havok

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/havok/havok.cpp:1909` | `havok_create_navmesh_shape` | `0x00834BC0` | havok_create_navmesh_shape dereferences a MOPP blob that failed to load (null code) |
| `src/volition/rfg/code/havok/havok.cpp:2288` | `havok_terrain_height` | `0x0082CF90` | havok_terrain_height (box form) returns false when scratch allocation fails even after the terrain was found. |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:22` | `havok_ragdoll_constrain_to_tag` | `0x0083EC10` | havok_ragdoll_constrain_to_tag pivots capsules at the sum of their two vertices instead of the middle |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:260` | `ragdoll_free_body` | `0x00818040` | Nothing sets body property 6, so a removed ragdoll's bodies, joints and handle slots stay in the world until the level ends *Note: Also port_history/src_status.md:268.* |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:260` | `ragdoll_free_body` | `0x00818040` | Nothing sets body property 6, which the ragdoll free path reads as the handle, so removed ragdoll bodies are never freed *Note: The reader of property 6 was checked; the absence of writers was not rescanned here.* |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:2645` | `havok_ragdoll_constrain_to_tag` | `0x0083EC10` | havok_ragdoll_constrain_to_tag leaks one reference to its ball-and-socket data per constraint |

### rfg/hud

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/hud/fullscreen_map.cpp:613` | `fsm_determine_zones_to_load` | `0x00867AF0` | fsm_determine_zones_to_load limits start_z with 2810.5 against a test of 3066. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:1209` | `fsm_render_corner_info` | `0x008527C0` | fsm_render_corner_info adds the window's min.x and min.y unevenly. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:1664` | `fsm_render_territories` | `0x00853530` | fsm_render_territories adds the window's min.x to the left twice and min.y only on the right corners. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:2286` | `fsm_process_mission_info` | `0x00854460` | fsm_process_mission_info runs off data_group_items past 6 missions and does not test a null item. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:2787` | `fullscreen_map_init_streaming_info` | `0x00868020` | fullscreen_map_init_streaming_info passes the territory name as a format string. |
| `src/volition/rfg/code/hud/hud.cpp:234` | `parse_hud_data` | `0x008416E0` | Hud_slots is filled from the table without a bound |
| `src/volition/rfg/code/hud/hud.cpp:270` | `parse_recruit_data` | `0x008417E0` | Follower_slots is filled from the table without a bound |
| `src/volition/rfg/code/hud/hud.cpp:568` | `hud_render` | `0x0087F400` | hud_render's respawn line uses one text for every reason and a negated, subtracted screen fraction |
| `src/volition/rfg/code/hud/hud_heartbeat_effect.cpp:158` | `heartbeat_load_data` | `0x008433A0` | Heartbeat_table is filled from the table without a bound |
| `src/volition/rfg/code/hud/hud_ui_activity_complete.cpp:803` | `hud_ui_activity_complete_start` | `0x00846670` | hud_ui_activity_complete_start keeps the previous activity's type for a mode it does not know |
| `src/volition/rfg/code/hud/hud_ui_district_liberation.cpp:170` | `hud_ui_district_liberation_process_input` | `0x00846910` | A non-liberation screen with no unlocks goes on to a page that reads a stale Unlocks_list |
| `src/volition/rfg/code/hud/hud_ui_district_liberation.cpp:670` | `hud_ui_district_liberation_level_shutdown` | `0x008471A0` | hud_ui_district_liberation_level_shutdown leaves In_liberation_screen set if it drops a pending screen, so its gates stay shut |
| `src/volition/rfg/code/hud/hud_ui_handbook.cpp:366` | `hud_ui_handbook_count_viewable_children` | `0x00847380` | hud_ui_handbook_count_viewable_children tests only direct children for being unlocked; descendants always count. |
| `src/volition/rfg/code/hud/hud_ui_handbook.cpp:658` | `hud_ui_handbook_load_table_data` | `0x00876FF0` | hud_ui_handbook_load_table_data returns on a full pool without closing the table. |
| `src/volition/rfg/code/hud/hud_ui_handbook.cpp:989` | `hud_ui_handbook_process_video` | `0x00857310` | The handbook video rescales the table's stored volume on every play and reads through a null line_info. |
| `src/volition/rfg/code/hud/hud_ui_mission_briefing.cpp:608` | `humb_process_bink_video` | `0x008580F0` | The mission briefing rescales its stored Bink volume by the volume scalar on every play. |
| `src/volition/rfg/code/hud/hud_ui_mission_briefing.cpp:670` | `hud_ui_mission_briefing_add_mission_line_info` | `0x00880C00` | hud_ui_mission_briefing_add_mission_line_info writes past the 40-entry array when full, since add only reports an error. |
| `src/volition/rfg/code/hud/hud_ui_mission_briefing.cpp:789` | `hud_ui_mission_briefing_init_lines` | `0x00880E40` | hud_ui_mission_briefing_init_lines reads through a null Line or Persona element. |
| `src/volition/rfg/code/hud/hud_ui_mission_complete.cpp:205` | `process_mission_information` | `0x00858CD0` | gfm_get_district_morale_percent is asked with a null district when the mission's district is missing |
| `src/volition/rfg/code/hud/hud_ui_mission_complete.cpp:429` | `hud_ui_mission_complete_exit` | `0x0084ABD0` | The mission results exit relinquishes control id 0 a second time |
| `src/volition/rfg/code/hud/hud_ui_mission_complete.cpp:457` | `hud_ui_mission_complete_hint_click` | `0x0084ACE0` | The hint calls pass the text before the image, against ui_mouse_over_hint_combo's parameter names |
| `src/volition/rfg/code/hud/hud_ui_upgrades.cpp:405` | `hud_ui_upgrades_create_header_data` | `0x0084AFC0` | The upgrades header template has 10 elements while process_header sets an eleventh. |
| `src/volition/rfg/code/hud/hud_ui_upgrades.cpp:539` | `hud_ui_upgrades_process_buttons` | `0x00859D70` | hud_ui_upgrades_process_buttons reads list rows past the end for rows beyond the list. |
| `src/volition/rfg/code/hud/in_world_gps.cpp:117` | `in_world_gps_register_path` | `0x0084BCA0` | in_world_gps_register_path writes segments[-1] when no seam was found. |
| `src/volition/rfg/code/hud/in_world_gps.cpp:142` | `in_world_gps_register_path` | `0x0084BCA0` | in_world_gps_register_path indexes the route's points with the knot number instead of segment.start. |
| `src/volition/rfg/code/hud/in_world_gps_shared.cpp:36` | `in_world_gps_shared_system_init` | `0x0084C250` | in_world_gps_shared_system_init tests the flag it sets, so its body never runs. |
| `src/volition/rfg/code/hud/minimap.cpp:307` | `cartographer_mover_collect_cb` | `0x0085B4D0` | an important building's map center is bmin (bmin - bmin) instead of the box center |
| `src/volition/rfg/code/hud/minimap.cpp:2447` | `cartographer::render_structures` | `0x0087D2E0` | render_structures keeps masked movers in 32-entry arrays with no check, for up to 512 movers |
| `src/volition/rfg/code/hud/minimap.cpp:3175` | `cartographer::add_patrol_path` | `0x00881350` | a patrol path of 512 nodes skips the last middle point and leaves a gap at the spline end |
| `src/volition/rfg/code/hud/minimap.cpp:3409` | `cartographer::render_patrol_routes` | `0x00881700` | a second patrol removal in a frame takes the wrong route because the removals shift later indices |
| `src/volition/rfg/code/hud/minimap.cpp:5867` | `cartographer::get_district_boundry` | `0x00863900` | the district boundary lookups accept index == size *Note: also get_district_boundry_point 0x00863940* |
| `src/volition/rfg/code/hud/objectives.cpp:90` | `load_objectives` | `0x008829D0` | load_objectives returns early on a missing element, leaving the pool's mark unrestored. |
| `src/volition/rfg/code/hud/objectives.cpp:130` | `init_dlc_objectives` | `0x00882B60` | init_dlc_objectives returns early on a missing element, leaving the pool's mark unrestored. |
| `src/volition/rfg/code/hud/objectives.cpp:174` | `create_objective` | `0x008818E0` | create_objective copies a description of up to 255 characters into a 128-byte stack buffer. |

### rfg/interface

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/interface/credits.cpp:449` | `credits_enter` | `0x009096B0` | credits_enter clears the file buffer before testing it for null. |
| `src/volition/rfg/code/interface/credits.cpp:541` | `credits_process` | `0x009127D0` | credits_process takes two steps off the fade count when leaving without a warp, where one was added. |
| `src/volition/rfg/code/interface/ingame/interface.cpp:421` | `letterbox_process` | `0x008A6BB0` | letterbox_process computes the fade only while its timer is not set |
| `src/volition/rfg/code/interface/ingame/interface.cpp:586` | `interface_replace_special_font` | `0x00884960` | interface_replace_special_font loops forever on an unknown INT_ token |
| `src/volition/rfg/code/interface/loading.cpp:1755` | `loading_screen_parse_tip` | `0x00888760` | A loading tip of an unknown type is accepted with type -1 |
| `src/volition/rfg/code/interface/loading.cpp:2087` | `loading_screen_process_ui_data` | `0x008F0C80` | The loading screen bounds an overlay index by the image count instead of the overlay count |
| `src/volition/rfg/code/interface/loading.cpp:2392` | `loading_screen_select_loading_images` | `0x008AB090` | A duplicate loading screen overlay index wraps at the image name count, not the overlay count |
| `src/volition/rfg/code/interface/main_menu.cpp:384` | `mm_process_bink_video` | `0x008CF160` | A failed background video play keeps 12 MB of the pregame split locked |
| `src/volition/rfg/code/interface/menu_transitions.cpp:121` | `wipe_right_in_process` | `0x008ABDB0` | The wipe right in transition never counts its stamp down, so it never ends alone |
| `src/volition/rfg/code/interface/menu_transitions.cpp:157` | `wipe_left_in_process` | `0x008ABF00` | The wipe left in transition never counts its stamp down |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp:7` | `cmm_process_data_item_playlist_info` | `0x008DF7E0` | a missing level or mode name crashes the playlist info (wcslen on null) |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp:842` | `ui_document_mp_change_matchmaking_enter` | `0x0090B370` | Menu_options of the playlist list overflows past 16 playlists |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.cpp:557` | `mpps_process_data_group_menu_options` | `0x008E0CA0` | A sledgehammer without a display name keeps the option's own text |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp:629` | `mm_process_input` | `0x0090C7A0` | the matchmaking sorted_player_list is never filled, so its player options box is unreachable |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp:923` | `compare_search_region` | `0x0088C650` | compare_search_region is not a consistent ordering for qsort |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp:1080` | `ui_document_mp_matchmaking_exit` | `0x008D3270` | the shell document stays loaded after these menus exit *Note: also party lobby, tutorials, change matchmaking; see their exit functions* |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.cpp:1432` | `party_lobby_get_prev_type` | `0x0088CE60` | party_lobby_get_prev_type goes forward instead of backward |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp:243` | `change_request` | `0x0088D770` | change_request ignores its filter argument when the scoreboard changes *Note: same copy in ui_document_menu_wrecking_crew_scoreboard.cpp* |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp:282` | `sort_friends` | `0x0088D7E0` | sort_friends compares an equal rank as greater both ways, not a consistent ordering for qsort *Note: same copy in ui_document_menu_wrecking_crew_scoreboard.cpp* |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp:435` | `scoreboard_load_global_data_items` | `0x008D3E70` | a failed scoreboard data item create reuses the previous handle |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp:506` | `tutorials_process_video` | `0x008AED00` | the tutorial video's volume is scaled in the shared table, so it shrinks with each play |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp:809` | `tutorials_process_loaded` | `0x008D5860` | the tutorials menu selection modulo divides by zero with an empty list |
| `src/volition/rfg/code/interface/option_menus/controller_options.cpp:363` | `control_config_select_cb` | `0x0088FEC0` | The second binding slot of a button binds its stick direction instead of the alternate button |
| `src/volition/rfg/code/interface/option_menus/controller_options.cpp:1875` | `controller_options_process_input` | `0x00900800` | The controller options' config buttons check the focus of input id -1, not this menu's |
| `src/volition/rfg/code/interface/option_menus/controller_options.cpp:2151` | `controller_options_exit` | `0x008F3C50` | Outside wrecking crew, controller_options_exit still copies the controls into Wc_player_infos[0] in every game. Also: controller_options_exit copies the controls into Wc_player_infos[0] in every game, even outside wrecking crew |
| `src/volition/rfg/code/interface/option_menus/ingame_options.cpp:443` | `ingame_options_create_menu` | `0x00917E90` | The pause menu shows cancel mission only for missions that were not autostarted |
| `src/volition/rfg/code/interface/option_menus/options_display.cpp:1117` | `display_options_are_changes_different_from_defaults` | `0x008AF9A0` | display_options_are_changes_different_from_defaults recomputes the pending rows into the screen size and rate indices |
| `src/volition/rfg/code/interface/option_menus/sony_options.cpp:59` | `sony_options_enter` | `0x0090FDF0` | sony_options_enter prints its Int3 and pops the state on every entry, then carries on setting up. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:661` | `setup_text_names` | `0x008B0DB0` | setup_text_names clears the first fade out's time when the second fade out is missing *Note: inlined in ui_setup_primary_secondary_text_names* |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:1728` | `hud_ui_cancel_radio_message_sounds` | `0x00895450` | a radio cancel unpauses the outro sound instead of stopping it |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:2538` | `ui_add_secondary_message` | `0x008D8270` | the secondary message animation adds the wrong animation's fade times to the display time |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:2555` | `ui_add_user_message` | `0x008D83D0` | user message text is passed to the formatter as the format string *Note: also ui_change_user_message 0x008D88D0 (anchor occurs twice)* |
| `src/volition/rfg/code/interface/ui_document_hud_missions.cpp:908` | `hud_ui_missions_get_activity_area_defense_info` | `0x00896B50` | The area defense mission info falls back to the riding shotgun title. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:157` | `Multi_doc_info` | `0x01664968` | King of the Hill's HUD document entry loads rfg_ui_mp_siege |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:1751` | `hud_ui_multiplayer_player_died` | `0x008EAB00` | an unknown kill phrase token drops the text up to the next space |
| `src/volition/rfg/code/interface/ui_document_hud_rail_driver.cpp:197` | `hud_ui_rail_driver_unload_items` | `0x008F5430` | hud_ui_rail_driver_unload_items does not test Active_uirail on a full unload, so a second one unloads handle -1. |
| `src/volition/rfg/code/interface/ui_document_hud_rail_driver.cpp:273` | `hud_ui_rail_driver_process` | `0x008F5540` | The rail driver x-ray effect starts a second time in the frame the assets arrive. |
| `src/volition/rfg/code/interface/ui_document_hud_tool_tips.cpp:444` | `ui_tool_tip_process_info` | `0x008EB900` | the tool tip's duration, already in seconds, is divided by 1000 again |
| `src/volition/rfg/code/interface/ui_document_hud_tool_tips.cpp:472` | `ui_tool_tip_process_info` | `0x008EB900` | the resume text replacement loop counts source characters and copies past the terminator |
| `src/volition/rfg/code/interface/ui_document_hud_weapons.cpp:113` | `hud_ui_weapons_add_weapon_show_data_items_info` | `0x00898DC0` | WeaponShowInfo has 2 template elements while 4 are set |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:269` | `wc_hud_item_timer_process` | `0x008997C0` | Crossing one minute left in a turn plays no sound, though the branch exists. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:326` | `wc_hud_item_players_process` | `0x008B3D70` | The player name is encoded into a 0x60-byte buffer passed as 0x180 bytes. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:456` | `wc_hud_item_stress_process` | `0x00899EB0` | The stress break shows only the seconds; the minutes are dropped. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:711` | `wc_hud_item_splash_barrels_process` | `0x0089A5A0` | The splash barrels item writes five rows (23 elements) into its 15-element template. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:851` | `wc_hud_item_splash_veto_process` | `0x008EC3D0` | The weapon veto prompt also needs wrecking_crew_player_can_veto; the backpack prompt does not. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:871` | `wc_hud_item_splash_veto_process` | `0x008EC3D0` | The veto screen reads Wc_player_infos[Wrecking_crew_current_player] without checking for -1. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:1049` | `hud_ui_wrecking_crew_process` | `0x008F5E40` | The wrecking crew document init callbacks run each time the assets finish loading, unlike the data items. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:1131` | `hud_ui_wrecking_crew_get_doc_handle` | `0x0089A900` | hud_ui_wrecking_crew_get_doc_handle(HWCD_NONE) reads before the handle table. |
| `src/volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp:1195` | `hud_ui_wrecking_crew_show_counter` | `0x0089AAD0` | hud_ui_wrecking_crew_show_counter does not mark the counter updated, unlike the stamp and the message. |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:803` | `ui_obtain_bmp_sheet_info` | `0x008C6FB0` | The project's first bitmap sheet peg name is added once per sheet instead of each sheet's name |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:1533` | `ui_get_usage_character_height` | `0x0089BB10` | A [scale:] tag scales only the width; the height function parses it and ignores it |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:2711` | `ui_relinquish_input_id` | `0x008B4D40` | ui_relinquish_input_id looks for the next lock one entry too low |
| `src/volition/rfg/code/interface/ui_document_save_load.cpp:7` | `hud_ui_save_load_proccess_hints` | `0x008C94B0` | The hint visible property is set with a null string instead of a false value to hide an object |
| `src/volition/rfg/code/interface/ui_document_save_load.cpp:9` | `hud_ui_save_load_get_list` | `0x0089DDB0` | The save list is re-sorted while a write requested by this screen may still point into the list |
| `src/volition/rfg/code/interface/ui_document_save_load.cpp:235` | `qsort_imported_save_data` | `0x0089DAF0` | Above 16 rows the scan down from the high end moves the slot pointer up (0x0089DBB0), so imported save slots no longer follow their infos and the pointer can leave the array. The port keeps the logic with a larger array (deviations.md). |
| `src/volition/rfg/code/interface/ui_document_stats.cpp:500` | `ui_stats_process_stats` | `0x0089EB40` | The stats screen passes the weapon's name as a format string. |
| `src/volition/rfg/code/interface/ui_document_view_dlc.cpp:745` | `ui_view_dlc_process_row_data` | `0x0089F8D0` | ui_view_dlc_process_row_data calls through a null group after a test that lets it through. |
| `src/volition/rfg/code/interface/ui_menu.cpp:1664` | `ui_menu::enable_menu_item` | `0x008B83E0` | enable_menu_item moves the selection when the selected item was enabled before, also when re-enabling it |
| `src/volition/rfg/code/interface/ui_menu.cpp:1787` | `ui_menu::add_function_item` | `0x008B84A0` | ui_menu add_function_item and increment items are bounded by the pools (32/48), not the 16 item slots *Note: Increment bound seen at 0x008B8510 (add_int_item).* |
| `src/volition/rfg/code/interface/ui_menu.cpp:2051` | `ui_menu::add_hint` | `0x008CBA50` | ui_menu::add_hint reads the new index before removing the old hint, so a replaced hint's info lands one slot past |
| `src/volition/rfg/code/interface/ui_message_box.cpp:297` | `ui_init_debug_text_items` | `0x008CC3E0` | ui_init_debug_text_items dereferences a missing debug text object |
| `src/volition/rfg/code/interface/ui_message_box.cpp:673` | `ui_process_utility_data` | `0x008DCAE0` | ui_process_utility_data reads a message box it has just freed |
| `src/volition/rfg/code/interface/ui_message_box.cpp:924` | `ui_process_voice_chat_data` | `0x008EFBF0` | Remote voice chat speakers get the local player's team color |
| `src/volition/rfg/code/interface/ui_message_box.cpp:959` | `ui_process_voice_chat_data` | `0x008EFF5D` | The voice chat speaker list's line break depends on the list slot, not on being the first speaker |
| `src/volition/rfg/code/interface/ui_message_box.cpp:1460` | `ui_msgbox_add_caption_item` | `0x008F06E0` | ui_msgbox_add_caption_item disables the item one past the new one, passing the item count as the handle |
| `src/volition/rfg/code/interface/vint/vint_rfg.cpp:242` | `get_localized_data` | `0x008A1D60` | get_localized_data prints the literal string true for every bool, whatever its value |

### rfg/level

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/level/undergrowth.cpp:761` | `destroy_zone` | `0x00933490` | destroy_zone never frees its layers' cell grids from Undergrowth_mempool |

### rfg/multi

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.cpp:543` | `multi_session::platform_process_operation` | `0x009E3230` | Removing a connection during the generic migration clears its next pointer, ending the walk |
| `src/volition/rfg/code/multi/multi_core/generic/multi_voice_generic.cpp:188` | `multi_voice_platform_remote_player_is_talking` | `0x00942D90` | failed voice info queries read the uninitialized info struct *Note: also multi_voice_platform_local_player_is_talking 0x009715B0* |
| `src/volition/rfg/code/multi/multi_core/global/multi.cpp:591` | `multi_shutdown` | `0x00A00D40` | multi_shutdown looks mp_wep up as an in-memory wavebank though it was loaded as streaming, so it is never unloaded Also: multi_shutdown's mp_wep lookup uses the wrong bank type, so the bank stays loaded across multiplayer sessions |
| `src/volition/rfg/code/multi/multi_core/global/multi.cpp:591` | `multi_shutdown` | `0x00A00D40` | multi_shutdown looks up mp_wep as bank type 1 though it was loaded as type 2, so the lookup fails and the bank stays loaded |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:712` | `multi_session_base::set_state` | `0x009F7340` | The MSS_NONE cleanup disconnects connections remove_connection already freed |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:772` | `multi_session_base::set_state` | `0x009F7340` | Re-entering MSS_STARTING as a waiting client goes back to READY |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1080` | `multi_session_base::set_creation_state` | `0x009E4100` | A failed host add deletes the host connection twice when host and me are the same connection |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1169` | `multi_session_base::process_joining` | `0x009E45A0` | The merge join's waiting step returns SOS_FAILED instead of in progress |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1742` | `multi_session_base::process_starting` | `0x009E4EC0` | process_starting and set_modification_state read m_connections without a null check *Note: Anchor occurs in several functions.* |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:2736` | `multi_session_base::join_received_info_response` | `0x009E5280` | A data response without info hands nullptr to copy, which dereferences it |
| `src/volition/rfg/code/multi/multi_core/global/multi_join.cpp:268` | `multi_join_process_join_request_packet` | `0x009DB070` | A join request for a reserved slot without a reservation is dropped with no answer |
| `src/volition/rfg/code/multi/multi_core/global/multi_join.cpp:275` | `multi_join_process_join_request_packet` | `0x009DB070` | With a reservation the join request's party uids are not read, misaligning the game data that follows a party |
| `src/volition/rfg/code/multi/multi_core/global/multi_session.cpp:129` | `multi_session_is_session_lobby_session` | `0x0094B950` | multi_session_is_session_lobby_session tests ST_GAME |
| `src/volition/rfg/code/multi/multi_core/global/multi_voice.cpp:212` | `update_voice_info` | `0x009B89C0` | update_voice_info's session type table has three entries for two types, visiting the party twice |
| `src/volition/rfg/code/multi/multi_core/global/multi_voice.cpp:608` | `multi_voice_handle_voice_chat_at_game_start_and_end` | `0x009AAC80` | the mute loop does not check a connection's game data for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.cpp:1247` | `bagman_mode_get_mode_specific_stat_value` | `0x00974E00` | the mode stat value texts dereference the player stats without a null check *Note: also CTF, KOTH, deathmatch, demolition, siege stat values* |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.cpp:806` | `capture_the_flag_mode_process` | `0x009E7A50` | after a touch return destroys the flag dummy, the CTF process still uses the destroyed dummy |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.cpp:1201` | `capture_the_flag_mode_player_in_flag_capture_zone` | `0x009E8FE0` | the CTF capture zone check dereferences the player without a null check |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.cpp:401` | `multi_game_mode_move_to_view_results_state` | `0x009E9080` | the player score log passes checksum 0 and does not check the stats for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.cpp:1437` | `multi_game_mode_award_kill_bonus_events` | `0x009EA1C0` | the kill bonus reads both players' camera orientations without a null check *Note: split out of multi_game_mode_player_died_common* |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_deathmatch.cpp:170` | `deathmatch_mode_player_died` | `0x009EBBE0` | deathmatch_mode_player_died does not check the killer for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp:304` | `activate_vip` | `0x009BA4A0` | activate_vip shows the new engineer objective only on the engineer's own machine |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp:549` | `swap_vip` | `0x009BAA50` | the server does not check that a swap request comes from the engineer |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp:633` | `score_has_been_updated` | `0x009EC090` | demolition score update loops forever when an experience event's parameter is zero |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp:980` | `demolition_mode_player_died` | `0x009EC960` | demolition_mode_player_died does not check the killer or its stats for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_demolition.cpp:1427` | `demolition_mode_get_mode_specific_stat_value` | `0x009772B0` | the demolition stat values do not check the stats for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp:158` | `king_of_the_hill_hud_icon_callback` | `0x00994250` | the King of the Hill icon callback uses the building's percent_destroyed without a null check |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp:452` | `king_of_the_hill_mode_level_init_post` | `0x009944A0` | the first King of the Hill marker without a building ends the target list |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp:557` | `king_of_the_hill_mode_process` | `0x009EDB50` | King of the Hill's process calls percent_destroyed on the building without a null check |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:90` | `siege_mode_level_init_post` | `0x009AB6C0` | Num_targets_on_priority has 4 entries but is indexed by an unchecked marker priority |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:630` | `siege_mode_level_init_post` | `0x009AB6C0` | a siege marker without a building keeps its pool slot and icon and is overwritten by the next target |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:783` | `siege_mode_process` | `0x009EF260` | the target destroyed log passes the timer's milliseconds as the seconds value |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:833` | `siege_mode_handshake` | `0x00994CF0` | siege_mode_handshake reads Siege_targets[0] even when there are no targets |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:834` | `siege_mode_handshake` | `0x00994CF0` | the siege handshake multiplies the pulse into the outline color once per outlined target, compounding it |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:993` | `siege_mode_round_begin` | `0x00978060` | siege_mode_round_begin stops at the first locked target without a building |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:1079` | `siege_mode_server_migration_post` | `0x009EFD30` | server_migration_post calls siege_mode_event_target_destroyed for destroyed targets but it returns at once for them |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:1337` | `siege_mode_get_mode_specific_stat_name` | `0x00978730` | the siege stat name does not check the experience event for null |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_siege.cpp:1357` | `siege_mode_get_mode_specific_stat_value` | `0x009787A0` | the siege stat value does not check the stats for null |
| `src/volition/rfg/code/multi/multi_game/multi_game_announcer.cpp:81` | `mga_check_removal` | `0x0094EF60` | mga_check_removal moves on after a removal, so the line shifted into place is skipped |
| `src/volition/rfg/code/multi/multi_game/multi_game_backpack_rack.cpp:665` | `multi_game_backpack_rack_process_sync` | `0x009BC510` | A rack sync packet stops at the first unknown rack |
| `src/volition/rfg/code/multi/multi_game/multi_game_badge.cpp:200` | `multi_game_badge_field_is_valid` | `0x009799C0` | multi_game_badge_field_is_valid reads a null field after its null guard |
| `src/volition/rfg/code/multi/multi_game/multi_game_badge.cpp:337` | `multi_badge_get_color_from_scheme` | `0x00995C20` | a color scheme value out of range ends the color search instead of skipping the field |
| `src/volition/rfg/code/multi/multi_game/multi_game_camera.cpp:460` | `get_next_player` | `0x009966D0` | get_next_player divides by the player list size without checking for an empty list |
| `src/volition/rfg/code/multi/multi_game/multi_game_camera.cpp:600` | `score_aquarium_marker` | `0x009AC470` | score_aquarium_marker keeps only the nearest player (the count stays at one) |
| `src/volition/rfg/code/multi/multi_game/multi_game_camera.cpp:943` | `process_mode_selection` | `0x00950660` | The camera scroll bar's back adds one to the mode, so it never wraps |
| `src/volition/rfg/code/multi/multi_game/multi_game_camera.cpp:1594` | `action_mode_should_switch_camera` | `0x009ADCA0` | The action camera's tracked player is not null-checked |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:487` | `multi_destruction_decompress_slice` | `0x009C0920` | The slice decompression only runs in multiplayer, unlike the other decompressors |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:1384` | `process_destruction_event_sp_array` | `0x009C3F70` | sp_array keys are widened without turning 0xFFFF back into -1 |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:1974` | `multi_game_destruction_frame_server_finalize_packet` | `0x009C46B0` | A destruction frame's event count has 3 bits, so more than 7 events in one frame are sent modulo 8 |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:1981` | `multi_game_destruction_frame_server_finalize_packet` | `0x009C46B0` | The destruction counts header is written to bytes 5-7 whatever the packet header size |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2215` | `get_frame_data` | `0x00955570` | A received destruction frame copies 512 bytes whatever its size, past the end of a shorter packet |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2314` | `add_destruction_events_from_packet` | `0x009C4920` | More than 20 shardify records (the 5-bit count allows 31) overrun the buffer and record array |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2494` | `multi_game_destruction_frame_client_process_destroyed_rfg_mover_remote_objects` | `0x009C4D80` | The destroyed rfg_movers' network handles are read and their remote objects looked up but not used |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2569` | `add_destruction_frame_to_list` | `0x00955730` | A frame already in the used list is lost when it is older than every stored frame |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2631` | `multi_game_destruction_clear_vehicle_rfg_mover_destruction_effect_info` | `0x009558A0` | The vehicle effect cleanup moves one element too few, losing the last active effect when an earlier one is dropped |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction.cpp:2723` | `multi_game_validate_destruction` | `0x009C4EB0` | The validation reader skips 8 bytes for an unowned piece the sender wrote in 3 |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction_migration.cpp:58` | `multi_game_process_destruct_update_server` | `0x0097B7F0` | the server's last sent frame is updated with a plain compare, without wrap-around |
| `src/volition/rfg/code/multi/multi_game/multi_game_destruction_migration.cpp:126` | `multi_game_process_migrate_req_df_update` | `0x009C5860` | a missing frame in the destruction updates copies from null with the previous byte count *Note: also multi_game_migration_send_client_destruction_update 0x009C5980* |
| `src/volition/rfg/code/multi/multi_game/multi_game_lobby.cpp:334` | `assign_teams_by_whole_groups` | `0x00956410` | the team balance loop's close-enough check is the wrong way round and stops the search early |
| `src/volition/rfg/code/multi/multi_game/multi_game_lobby.cpp:534` | `find_best_team_assignments` | `0x009AF280` | the group split reads the second half's size from the wrong group |
| `src/volition/rfg/code/multi/multi_game/multi_game_lobby.cpp:574` | `search_team_combinations` | `0x00999120` | search_team_combinations' group split is unreachable because the loop leaves the count at the checked limit |
| `src/volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.cpp:12` | `Query_type_names` | `0x0167F008` | QT_ALLGAMES has no name, its Query_type_names entry is null |
| `src/volition/rfg/code/multi/multi_game/multi_game_matchmaking_internal.cpp:123` | `multi_game_matchmaking_internal_get_query_type_name` | `0x00956F30` | the query type name lookups index Query_type_names without a range check *Note: also game_query_base::query_type_name 0x0097C100* |
| `src/volition/rfg/code/multi/multi_game/multi_game_migration.cpp:158` | `destroy_client_remote_objects` | `0x009C6830` | with a null local player, destroy_client_remote_objects keeps a player-type remote object that is not a player |
| `src/volition/rfg/code/multi/multi_game/multi_game_migration.cpp:639` | `multi_game_process_migrate_last_df` | `0x00957020` | multi_game_process_migrate_last_df dereferences the connection without a null check |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:136` | `(fuel effect statics)` | `0x00000000` | The three backpack fuel effect infos are never set in rfg.exe, so the fuel effect plays a null effect *Note: Absence of a writer not re-checked by xref.* |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:140` | `(arc tension static)` | `0x00000000` | The arc spline tension is never set in rfg.exe and stays 0 *Note: Absence of a writer not re-checked by xref.* |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:3288` | `multi_object_backpack::process` | `0x009F2A70` | A rhino hit without an object ends the backpack's process for the frame |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:3355` | `multi_object_backpack::process` | `0x009F2A70` | The rhino's walker hits go into the npc list without a bound |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:4056` | `multi_game_backpack_process_ripple_hits` | `0x009F44B0` | Up to 255 ripple hits are read into a 16-entry array |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_flag.cpp:290` | `multi_object_flag::event_dropped` | `0x0097D610` | multi_object_flag::event_dropped calls remove_child on the carrier without a null check |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_marker.cpp:201` | `multi_object_marker::serialize` | `0x0099BA50` | A marker's team is read twice; the second read wins whatever the mode |
| `src/volition/rfg/code/multi/multi_game/multi_game_party.cpp:75` | `multi_game_party_send_lobby_data_update` | `0x009CDF60` | the party's lobby data send does not check the playlist for null |
| `src/volition/rfg/code/multi/multi_game/multi_game_party.cpp:118` | `multi_game_party_check_send_lobby_data_update` | `0x009CE1A0` | a changed lobby sends the update to the old member list without refreshing it |
| `src/volition/rfg/code/multi/multi_game/multi_game_party.cpp:215` | `multi_game_party_system_process` | `0x00A04CD0` | multi_game_party_system_process reads Lobby_session before its null check |
| `src/volition/rfg/code/multi/multi_game/multi_game_party.cpp:338` | `multi_game_party_join_party` | `0x00A00AB0` | a game lobby invite with no playlists reads a null playlist |
| `src/volition/rfg/code/multi/multi_game/multi_game_party.cpp:462` | `multi_game_party_reset_last_received_lobby_data_update` | `0x00982070` | the party reset's memset also clears Current_party_lobby's vtable pointer |
| `src/volition/rfg/code/multi/multi_game/multi_game_player.cpp:2264` | `multi_game_player_process_player_ragdoll` | `0x009D1680` | The receiver decompresses the PT_PLAYER_RAGDOLL linear speed with bias 75 while the sender compresses with 50 |
| `src/volition/rfg/code/multi/multi_game/multi_game_player.cpp:2264` | `multi_game_player_process_player_ragdoll` | `0x009D1680` | The PT_PLAYER_RAGDOLL receiver decompresses linear speed with bias 75 while the sender compresses it with bias 50 |
| `src/volition/rfg/code/multi/multi_game/multi_game_playlists.cpp:133` | `parse_playlist_entry` | `0x009B1EF0` | a playlist entry's named options set is not checked for null |
| `src/volition/rfg/code/multi/multi_game/multi_game_preferences.cpp:146` | `multi_game_preferences_create_query_list` | `0x009839A0` | the query lists write a pair's second query without checking the room left *Note: same in multi_game_preferences_create_any_playlist_query_list* |
| `src/volition/rfg/code/multi/multi_game/multi_game_preferences.cpp:174` | `multi_game_preferences_create_any_playlist_query_list` | `0x00983B40` | the any-playlist query list gives only the first query of a pair a party size |
| `src/volition/rfg/code/multi/multi_game/multi_game_preferences.cpp:191` | `multi_game_preferences_get_random_query` | `0x00983CC0` | multi_game_preferences_get_random_query does not check the mode's playlist for null *Note: old comment said neither playlist was checked; the random pick's result is checked* |
| `src/volition/rfg/code/multi/multi_game/multi_game_preferences.cpp:336` | `multi_game_preferences_send_my_history` | `0x009D2320` | multi_game_preferences_send_my_history sends the copy at the target's index instead of our own |
| `src/volition/rfg/code/multi/multi_game/multi_game_preferences.cpp:362` | `multi_game_preferences_choose_random_preferred_playlist_entry` | `0x00960EC0` | the preferred entry choice does not check the current query's playlist for null |
| `src/volition/rfg/code/multi/multi_game/multi_game_render.cpp:259` | `add_hud_icon` | `0x009A0390` | add_hud_icon does not check hud_icon_add's result for null |
| `src/volition/rfg/code/multi/multi_game/multi_game_render.cpp:311` | `multi_game_process_player_names` | `0x009A04F0` | one player without game data ends the player names loop, skipping the players after it |
| `src/volition/rfg/code/multi/multi_game/multi_game_render.cpp:362` | `multi_game_render_get_team_icon` | `0x00961130` | the team icon of team 3 (marauder) reads past the two icons of Multi_team_icons |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:55` | `multi_game_stats (range checks)` | `0x009D33E0` | The stats index range checks accept 16 although Mp_player_stats has 16 entries |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:1233` | `multi_game_stats_get_player_stats` | `0x00965B10` | index 16 passes the stats range checks although Mp_player_stats has 16 entries *Note: same check inlined in the mode files, each commented* |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:1466` | `multi_game_stats_alter_player_xp` | `0x009D2D60` | the backpack variety count indexes the player's backpacks by the tracked entry instead of the entry's backpack |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:1563` | `multi_game_stats_read_player_score` | `0x00965C80` | the score read skips an out-of-range index without consuming its scores, desyncing the packet offset *Note: also multi_game_stats_read_team_score* |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:1678` | `multi_game_stats_endgame_sync` | `0x009D33E0` | multi_game_stats_endgame_sync writes our stats without checking the lookup's null result |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:1752` | `multi_game_stats_are_finalizing` | `0x009864F0` | multi_game_stats_are_finalizing is true only while some but not all flags in its walk are set |
| `src/volition/rfg/code/multi/multi_game/multi_game_weapons.cpp:102` | `mgw_send` | `0x009D3920` | Server relays bullet hits with multi_io_send (unreliable) while clients send them reliably |
| `src/volition/rfg/code/multi/multi_game/multi_game_weapons.cpp:462` | `multi_gw_process_bullet_hit` | `0x009D3D30` | Received weapon_fire_info leaves hit_alt_body_idx and hit_bone uninitialized; only hit_handle and w_info are set by the constructor |
| `src/volition/rfg/code/multi/multi_game/multi_game_weapons.cpp:493` | `multi_gw_process_bullet_hit` | `0x009D3D30` | Received PT_BULLET_HIT sets end_pos to hit_pos, so the kill blood splatter ray (end_pos - hit_pos) of a relayed hit has zero length |

### rfg/objects

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/objects/custom_object.cpp:595` | `windmill_blade_damaged_callback` | `0x00A522D0` | windmill blade damage starts the custom effects only when the process pool cannot take the blade |
| `src/volition/rfg/code/objects/custom_object.cpp:646` | `windmill_base_stream_in_callback` | `0x00A2CB50` | the windmill base keeps its motor handle in m_prefix_effects[0] of its properties |
| `src/volition/rfg/code/objects/custom_object.cpp:673` | `windmill_base_damaged_callback` | `0x00A52520` | windmill base damage starts the custom effects only when the process pool cannot take the motor |
| `src/volition/rfg/code/objects/custom_object.cpp:719` | `computer_stream_in_callback` | `0x00A2CBD0` | the computer's slot 2 effect plays whenever its handle is not 0, replacing and leaking a running effect |
| `src/volition/rfg/code/objects/custom_object.cpp:995` | `excavator_blade_stream_in_callback` | `0x00AD94A0` | the excavator blade's event registration leaks when it has no machine or is not upright |
| `src/volition/rfg/code/objects/custom_object.cpp:1084` | `fire_barrel_stream_in_callback` | `0x00A52810` | fire barrels start their effects without checking the pool add, even when the pool is full |
| `src/volition/rfg/code/objects/custom_object.cpp:1111` | `kill_on_move_stream_in_callback` | `0x00A528A0` | kill-on-move objects start their effects without checking the pool add, even when the pool is full |
| `src/volition/rfg/code/objects/custom_object.cpp:1413` | `custom_object_do_frame` | `0x00A2D2E0` | custom_object_do_frame advances the index after a removal, so the entry after a removed one waits a frame |
| `src/volition/rfg/code/objects/humans/human.cpp:2321` | `human_get_turret_state_anim` | `0x00AC6510` | human_get_turret_state_anim compares h_fov_neg with 3.1425927 where a negative value looks intended, so the test is nearly always true. |
| `src/volition/rfg/code/objects/humans/human.cpp:7640` | `human_has_ammo` | `0x00A7C860` | human_has_ammo asks for the charges before the null check of the human |
| `src/volition/rfg/code/objects/humans/human.cpp:11058` | `human::update_render` | `0x00A9BB90` | The head's early returns in update_render leave camera_alpha_override set instead of resetting it to 1. |
| `src/volition/rfg/code/objects/humans/human.cpp:11059` | `human::update_render` | `0x00A9BB90` | The LOD head lowers the body LOD by one, also from 0, so a head LOD of -1 reads before the sub mesh table. |
| `src/volition/rfg/code/objects/humans/human.cpp:11730` | `human_get_unanimated_hit_location` | `0x00A2EC40` | human_get_unanimated_hit_location rotates by the scaled bone matrix, so its result is the inverse only at scale 1 (bind_pos = scale^2 * true) |
| `src/volition/rfg/code/objects/humans/human_collide.cpp:123` | `human_collide_calculate_damage_with_timer` | `0x00A093B0` | Every hit within 500 ms of the previous one deals 0 because the stored damage is raised first, and each hit restarts the timer |
| `src/volition/rfg/code/objects/humans/human_combat.cpp:831` | `human_combat_init` | `0x00A09970` | human_combat_init resets last_killed_time_ms twice and never initializes last_kill_time_ms. |
| `src/volition/rfg/code/objects/humans/human_combat.cpp:1292` | `human_turn_towards_melee_target` | `0x00A54EF0` | The result of fl_angle_cap_radians is discarded, so the melee turn test uses the raw uncapped angle. |
| `src/volition/rfg/code/objects/humans/human_combat.cpp:1401` | `human_combat_process_melee` | `0x00AD23A0` | The NPC cover test comes right after cur_state is set to HS_FALL, so it is always false. |
| `src/volition/rfg/code/objects/humans/human_heads_and_props.cpp:132` | `human_hap_parse_head` | `0x00A2FE40` | human_hap_parse_head dereferences the base head and LOD head item infos without a null check. |
| `src/volition/rfg/code/objects/humans/human_info.cpp:537` | `parse_human_table_footsteps` | `0x00A30070` | Footstep list Name is copied into the 20-byte name with an unbounded copy. |
| `src/volition/rfg/code/objects/humans/human_info.cpp:851` | `human_info_stream_load` | `0x00A30470` | human_info_stream_load returns from the whole callback when a character already exists, skipping later humans and the heads. |
| `src/volition/rfg/code/objects/humans/human_info.cpp:885` | `human_info_stream_load` | `0x00A30470` | human_info_stream_load reads sub prop items without testing them for null, unlike the prop items. |
| `src/volition/rfg/code/objects/humans/human_move.cpp:551` | `human_move_force_direct_walk` | `0x00A0B220` | The cover check in human_move_force_direct_walk is dead, as cur_state was just set to standard |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice.cpp:471` | `pick_new_human_voice` | `0x00A572D0` | pick_new_human_voice tests the human's own voice each pass, so a human whose voice is unloaded gets none |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice.cpp:920` | `do_persona_streaming` | `0x00A31850` | do_persona_streaming leaves to_be_unloaded set on personas it has released |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice.cpp:1416` | `human_voice_persona::persona_play_voice_line_internal` | `0x00A584B0` | The first DLC subtitles marauder lines by the guerrilla group's subtitled flag |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice.cpp:1539` | `persona_attach_line_data` | `0x00A83180` | The DLC disable data is keyed by persona name but looked up by resource file name |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice.cpp:1907` | `parse_personas_flags_table` | `0x00A58170` | parse_personas_flags_table reads later flags only when the earlier ones exist |
| `src/volition/rfg/code/objects/humans/human_voice/human_voice_shared.cpp:431` | `human_voice_persona::parse_personas_table_shared` | `0x00A59FD0` | A failed assasm_container_start returns with the persona table open |
| `src/volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.cpp:230` | `voice_line_unlocking_choose` | `0x00A32D60` | voice_line_unlocking_choose accepts a handle equal to the group count, one past the end |
| `src/volition/rfg/code/objects/humans/human_weapons.cpp:561` | `human_weapons_get_state_animation` | `0x00A0E780` | Walking without ammo selects ANIMATION_STATE_RUN_NO_AMMO instead of a walking state. |
| `src/volition/rfg/code/objects/humans/human_weapons.cpp:627` | `human_weapons_get_state_animation` | `0x00A0E780` | Looking back while driving picks DRIVE_READY + 1, which is the left-looking state. |
| `src/volition/rfg/code/objects/humans/human_weapons.cpp:837` | `human_weapons_firearm_process_common` | `0x00ABE330` | Single-shot weapons call the single fire animation twice in the same fire step. |
| `src/volition/rfg/code/objects/humans/npc_sound_info.cpp:97` | `npc_sound_info_building_damage::is_relevant_to_npc` | `0x00A5B020` | npc_sound_info_building_damage::is_relevant_to_npc reads the building's team without a null check |
| `src/volition/rfg/code/objects/humans/npc_sound_info.cpp:339` | `npc_sound_info_silent_death::inform_npc` | `0x00A33760` | npc_sound_info_silent_death::inform_npc reads the target's position without a null check |
| `src/volition/rfg/code/objects/humans/player.cpp:2316` | `player_switch_seat` | `0x00A34200` | player_switch_seat keeps the patrol handle in pp's stack slot, so without a patrol vehicle it compares the pointer's bits with the vehicle handle. |
| `src/volition/rfg/code/objects/humans/player.cpp:4839` | `player_is_near_turret` | `0x00ACCE20` | player_is_near_turret compares blocking humans with the local player instead of pp. |
| `src/volition/rfg/code/objects/humans/player.cpp:5185` | `player_is_near_action_node` | `0x00AC1DF0` | player_is_near_action_node measures a disabled node with the previous node's entry point. *Note: First iteration reads uninit stack in rfg.exe; port starts at zero and says so.* |
| `src/volition/rfg/code/objects/humans/player.cpp:5191` | `player_is_near_action_node` | `0x00AC1DF0` | player_is_near_action_node inverts the player flag for a non-player human: the node must not allow players. |
| `src/volition/rfg/code/objects/humans/player.cpp:5633` | `player_use_action_node` | `0x00ABE5D0` | player_use_action_node calls action_node_reserve_node twice. |
| `src/volition/rfg/code/objects/humans/player.cpp:6233` | `ally_squad_hud_message` | `0x00A10180` | ally_squad_hud_message formats into a thread-local buffer nothing reads, so those squad messages never show |
| `src/volition/rfg/code/objects/humans/player.cpp:6531` | `player::disband_ally_and_escort_squads` | `0x00A9DDC0` | disband_ally_and_escort_squads never ends if a member handle stops naming an NPC |
| `src/volition/rfg/code/objects/humans/player.cpp:6677` | `player::check_squad` | `0x00A850D0` | check_squad's leaving_vip can never be set, as it is only taken on a path where the NPC is not a VIP |
| `src/volition/rfg/code/objects/humans/player_cover.cpp:168` | `cover_cast_ray` | `0x00A86C70` | cover_cast_ray reads a hit with no object through null |
| `src/volition/rfg/code/objects/humans/player_cover.cpp:326` | `player_cover_lean_out` | `0x00A875C0` | player_cover_lean_out reads a null weapon when Player_cover_force_fine_aim is set |
| `src/volition/rfg/code/objects/humans/player_cover.cpp:1615` | `player_cover_do_frame` | `0x00A9EC30` | A cover verify failure against a vehicle does not return, so the tests below it still run |
| `src/volition/rfg/code/objects/humans/player_hold_object.cpp:385` | `player_hold_object_process` | `0x00AC35E0` | player_hold_object_process uses rb for the throw after a null test that only guarded the earlier block. |
| `src/volition/rfg/code/objects/humans/player_meta_data.cpp:669` | `player_meta_data::mining_destroyed` | `0x00A11C90` | player_meta_data::mining_destroyed passes the game time played as the log event's duration too. *Note: Comment on the line above; also present in supply_crate_destroyed (0x00A11E00, not checked in the binary).* |
| `src/volition/rfg/code/objects/ladder.cpp:661` | `mp_should_force_dismount` | `0x00A5E4F0` | mp_should_force_dismount's arguments are swapped by LTCG (ladder passed in EAX) *Note: Calling convention quirk only, no behaviour difference.* |
| `src/volition/rfg/code/objects/ladder.cpp:699` | `ladder_process_human` | `0x00AC4520` | ladder_process_human disables a ladder without a building *Note: The port adds a null check on the ladder, commented there.* |
| `src/volition/rfg/code/objects/ladder.cpp:845` | `ladder_process_human` | `0x00AC4520` | The bottom rungs exit without an animation unless the human is moving down faster than 0.5 m/s |
| `src/volition/rfg/code/objects/object_action_node.cpp:450` | `object_action_node::is_allowed_to_use` | `0x00A155B0` | is_allowed_to_use refuses every NPC on a node whose flags allow the player |
| `src/volition/rfg/code/objects/object_action_node.cpp:1208` | `object_action_node::create_prop_object` | `0x00AA40C0` | create_prop_object applies only positive prop rotation offsets |
| `src/volition/rfg/code/objects/object_action_node.cpp:1323` | `object_action_node::play_ambient_action` | `0x00AA3D70` | play_ambient_action passes an action index to vanim_get_anim_length, which takes an alias |
| `src/volition/rfg/code/objects/object_action_node.cpp:1350` | `object_action_node::play_ambient_action` | `0x00AA3D70` | play_ambient_action starts the same action a second time with play flags 0x20 |
| `src/volition/rfg/code/objects/object_action_node.cpp:1718` | `same_action_node_info` | `0x00A38CF0` | same_action_node_info compares only the actions of the action list, ignoring their cooldowns |
| `src/volition/rfg/code/objects/object_action_node.cpp:1735` | `set_up_no_object_interaction_flag` | `0x00A38D90` | set_up_no_object_interaction_flag tests entry 0 of the anim list on every pass of its loop |
| `src/volition/rfg/code/objects/object_action_node.cpp:2020` | `action_node_definition::load` | `0x00A8C590` | action_node_definition::load returns true at once on a full anim list, skipping the hotspot effect and prop_required |
| `src/volition/rfg/code/objects/object_action_node.cpp:2092` | `load_action_node_definitions` | `0x00AA3190` | load_action_node_definitions frees a definition that failed a DLC reload while the hash still names it |
| `src/volition/rfg/code/objects/object_bftp_node.cpp:258` | `object_bftp_node::serialize` | `0x00AA4A40` | a save made with a bomb armed on the player's car stores that bomb's node as not collected |
| `src/volition/rfg/code/objects/object_bounding_box.cpp:103` | `object_bounding_box::serialize` | `0x00AA4E80` | object_bounding_box::serialize does not write the type back on save. |
| `src/volition/rfg/code/objects/object_bounding_box.cpp:139` | `object_bounding_box::update_pos_and_orient` | `0x00A15DD0` | object_bounding_box::update_pos_and_orient leaves bb where it was set on load. |
| `src/volition/rfg/code/objects/object_convoy.cpp:142` | `build_node_list` | `0x00A8DD90` | build_node_list caps a road point at num_points, not the last index |
| `src/volition/rfg/code/objects/object_convoy.cpp:887` | `object_convoy::get_dist_sq_to_nearest_following_member` | `0x00A39D10` | get_dist_sq_to_nearest_following_member's loop has an empty body, so it always answers -1 |
| `src/volition/rfg/code/objects/object_convoy.cpp:923` | `object_convoy::rotate_to_back` | `0x00A161D0` | rotate_to_back leaves a duplicate of the moved member when the next slot is free |
| `src/volition/rfg/code/objects/object_convoy.cpp:1563` | `convoy_create` | `0x00A8F100` | convoy_create reads through null when creation of the object fails |
| `src/volition/rfg/code/objects/object_cover_node.cpp:325` | `cover_node::free_this` | `0x00AA6560` | cover_node::free_this tells only the first human that reserved the node, and reads the handle after object::free_this |
| `src/volition/rfg/code/objects/object_cover_node.cpp:664` | `cover_node::is_disabled` | `0x005DBA80` | cover_node::is_disabled reports a node with no valid timer as disabled |
| `src/volition/rfg/code/objects/object_demolitions_master_node.cpp:124` | `demolitions_master_level_data::reset` | `0x00A33BD0` | demolitions_master_level_data::reset does not clear failure_string_time or the stats |
| `src/volition/rfg/code/objects/object_demolitions_master_node.cpp:151` | `load_upper_string` | `0x00A62E00` | The level table's string copies have no bound, so a string longer than its field overruns the next |
| `src/volition/rfg/code/objects/object_demolitions_master_node.cpp:408` | `object_demolitions_master_node::serialize_mode` | `0x00A17CC0` | object_demolitions_master_node::serialize_mode has no null check of its data |
| `src/volition/rfg/code/objects/object_dummy.cpp:22` | `Dummy_object_type_table` | `0x01319744` | The dummy type table misspells Tech Response; the rfg.exe key is "Tech Reponse Pos" |
| `src/volition/rfg/code/objects/object_dummy.cpp:202` | `object_dummy::create_dummy_object` | `0x00AA6FC0` | The reinforced fence dummy only reaches a debug trap and creates nothing |
| `src/volition/rfg/code/objects/object_guard_node.cpp:203` | `object_guard_node::release_node` | `0x00A180E0` | release_node keeps a live reservation for its owner; another handle or an expired one releases it |
| `src/volition/rfg/code/objects/object_item.cpp:361` | `item_high_res_stream_load` | `0x00A648B0` | item_high_res_stream_load and unload return at the first item whose mesh is not loaded, ending the container walk *Note: also item_high_res_stream_unload 0x00A649E0* |
| `src/volition/rfg/code/objects/object_item.cpp:2394` | `item::update_render` | `0x00A65190` | A held item that is not stealthed clears its distortion flag twice. |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:156` | `maybe_play_ambush_voice_line` | `0x00A66EA0` | maybe_play_ambush_voice_line reads the first member of a squad with no members, a stale entry |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:194` | `marauder_ambush_region::serialize` | `0x00AA9B40` | marauder_ambush_region::serialize saves enabled as 4 bytes and only loads it at 1 byte |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:195` | `marauder_ambush_region::serialize` | `0x00AA9B40` | marauder_ambush_region clears only two flag bits on load, so a reused pool slot keeps the rest |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:270` | `marauder_ambush_region::update_pos_and_orient` | `0x00A19D90` | marauder_ambush_region::update_pos_and_orient does not move bb, so get_bbox keeps the box from the load |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:282` | `marauder_ambush_region::get_radius` | `0x00A19E10` | marauder_ambush_region::get_radius is always 0, as serialize sets radius to 0 |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:357` | `marauder_ambush_region::trigger_ambush` | `0x00A9198D` | trigger_ambush reads only the first picked flag, so all of a region's nodes spawn if slot 0 was picked and none otherwise; refused picks still count |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:358` | `marauder_ambush_region::trigger_ambush` | `0x00A9198D` | trigger_ambush reads only the first picked flag, so all of a region's nodes spawn or none do |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:385` | `marauder_ambush_region::trigger_ambush` | `0x00A91820` | trigger_ambush writes past its 15 picked flags on a region with more than 15 nodes |
| `src/volition/rfg/code/objects/object_marauder_ambush_region.cpp:495` | `marauder_ambush_region::update_squad_orders` | `0x00A919D0` | update_squad_orders reads the first member of the opposing squad whatever the member count, a stale entry when it has none |
| `src/volition/rfg/code/objects/object_path_road.cpp:298` | `object_path_road::get_width` | `0x00A67D20` | object_path_road::get_width reads the width of the point after the last at the end of a road |
| `src/volition/rfg/code/objects/object_restricted_area.cpp:165` | `object_restricted_area::serialize` | `0x00AAAEB0` | object_restricted_area::serialize saves disabled as 4 bytes and only loads it at 1 byte |
| `src/volition/rfg/code/objects/object_restricted_area.cpp:166` | `object_restricted_area::serialize` | `0x00AAAEB0` | A box restricted area asks for a warning polygon it does not have |
| `src/volition/rfg/code/objects/object_restricted_area.cpp:236` | `object_restricted_area::stream` | `0x00A1B150` | object_restricted_area::stream reads its flag after destroy_object |
| `src/volition/rfg/code/objects/object_restricted_area.cpp:455` | `restricted_area_find_area` | `0x00A6A2E0` | restricted_area_find_area's include_disabled can never find anything, as both inside tests answer false for a disabled area |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:447` | `object_roadblock_node::place_bbox` | `0x00A3DC40` | place_bbox reads formation_info unchecked though barrier_pattern is optional, so a type without one crashes when started |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:756` | `object_roadblock_node::do_frame` | `0x00A92EF0` | object_roadblock_node::do_frame reads Player unchecked |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:849` | `object_roadblock_node::spawn_roadblock_dynamic` | `0x00ABA590` | spawn_roadblock_dynamic leaves active set when the bounding box fails |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1104` | `load_roadblock_types` | `0x00A92FB0` | load_roadblock_types returns without closing the table when a pool or the string pool is full |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1146` | `load_roadblock_formations` | `0x00A93070` | load_roadblock_formations returns without closing the table when a pool or the string pool is full |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1211` | `roadblock_system_init` | `0x00AAB490` | roadblock_system_init frees types without resetting them before clearing the squad pool |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1271` | `roadblock_spawn` | `0x00ABA9A0` | roadblock_spawn reads Player unchecked |
| `src/volition/rfg/code/objects/object_safehouse.cpp:859` | `safehouse_find_safehouse_by_grid_id` | `0x00A6B7C0` | safehouse_find_safehouse_by_grid_id keeps looking after a match, so the last safehouse in the cell wins |
| `src/volition/rfg/code/objects/object_spawn_region.cpp:160` | `object_spawn_region::serialize` | `0x00AACB60` | object_spawn_region's near box subtracts each corner from itself, so it is only the position |
| `src/volition/rfg/code/objects/object_squad.cpp:3229` | `squad_mission_clean_up` | `0x00A70D80` | squad_mission_clean_up reads through a null definition, and through a null player in its vehicle pass |
| `src/volition/rfg/code/objects/object_upgrade_node.cpp:97` | `object_upgrade_node::serialize` | `0x00AAD990` | object_upgrade_node::serialize compares the type name only when the property was not found, so every node stays type 0 |
| `src/volition/rfg/code/objects/projectile.cpp:1758` | `singularity_bomb_pull` | `0x00AE0B20` | The singularity bomb's mover ray direction is randomized in place, using the same vector as input and output. *Note: Inlined in process_singularity_bomb in rfg.exe.* |
| `src/volition/rfg/code/objects/projectile.cpp:1804` | `projectile::process_singularity_bomb` | `0x00AE0B20` | At exactly time_until_propelled the singularity bomb pulls once before it lifts. |
| `src/volition/rfg/code/objects/projectile.cpp:2382` | `projectile::mass_driver_collide` | `0x00AD7650` | mass_driver_collide compares the local player with itself before the HUD hit's death check. |
| `src/volition/rfg/code/objects/turret.cpp:340` | `turret::disable_fov_limits` | `0x00A72730` | disable_fov_limits reads through a null physics when the turret is not streamed in. *Note: Comment is inside the function.* |
| `src/volition/rfg/code/objects/turret.cpp:358` | `turret::set_h_fov_constraints` | `0x00A41A40` | set_h_fov_constraints reads through a null physics when the turret is not streamed in. *Note: Comment is the line above this anchor.* |
| `src/volition/rfg/code/objects/turret.cpp:365` | `turret::set_h_fov_constraints` | `0x00A41A40` | set_h_fov_constraints writes the middle hinge limits unnegated where assemble_components negates them. |
| `src/volition/rfg/code/objects/turret.cpp:374` | `turret::set_v_fov_constraints` | `0x00A41B00` | set_v_fov_constraints reads through a null physics when the turret is not streamed in. *Note: Comment is the line after this anchor.* |
| `src/volition/rfg/code/objects/turret.cpp:738` | `turret::stream` | `0x00AB4940` | turret::stream clears weapon callbacks only on cycling weapons, though load sets them on all. |
| `src/volition/rfg/code/objects/turret.cpp:1087` | `turret::process_bullet_hit` | `0x00AD8D50` | turret::process_bullet_hit passes the weapon's position in the operator's space as the world hit position. |
| `src/volition/rfg/code/objects/turret.cpp:1178` | `turret::calculate_aim_parameters` | `0x00A42040` | turret::calculate_aim_parameters moves the pitch at the heading's rotation speed. |
| `src/volition/rfg/code/objects/turret.cpp:1281` | `turret::calculate_aim_parameters` | `0x00A42040` | The auto return scales its step twice, once for heading and again for pitch, on the same variable. |
| `src/volition/rfg/code/objects/turret_info.cpp:146` | `parse_turret_info` | `0x00A1FB70` | A reload clears the vehicle component names and the !reload checks never set them again. |
| `src/volition/rfg/code/objects/turret_info.cpp:303` | `turret_info_read_table` | `0x00A207A0` | turret_info_read_table skips an oversized DLC table without closing it. |
| `src/volition/rfg/code/objects/turret_info.cpp:309` | `turret_info_read_table` | `0x00A207A0` | A DLC reload parses its table over the first base turret infos rather than the bundle's. |
| `src/volition/rfg/code/objects/turret_internal.cpp:89` | `change_to_component_space` | `0x00A209E0` | change_to_component_space rotates the matrix by itself instead of by the component's rotation. |
| `src/volition/rfg/code/objects/turret_internal.cpp:1491` | `turret_physics_play_alt_effect` | `0x00A20F80` | turret_physics_play_alt_effect plays the primary's effect for the secondary weapon. |
| `src/volition/rfg/code/objects/turret_internal.cpp:1623` | `turret_physics_process_door_weapon_unready` | `0x00A45650` | turret_physics_process_door_weapon_unready tests the door open cue id, then plays the close cue. |

### rfg/os

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/os/dc_menu.cpp:48` | `dc_menu_add_selection` | `0x00AE79F0` | dc_menu adds items and menus with no bounds checks against the 128 items and 18 menus |
| `src/volition/rfg/code/os/dlmalloc/dmalloc.cpp:5754` | `my_sbrk` | `0x00AE8590` | my_sbrk's heap end is the heap start plus Level_memmgr's maximum, not the 0xB9000-byte dlmalloc pool |
| `src/volition/rfg/code/os/dlmalloc/dmalloc.cpp:5754` | `my_sbrk` | `0x00AE8590` | my_sbrk's heap end is the heap start plus Level_memmgr's maximum size, not the dlmalloc pool's size |
| `src/volition/rfg/code/os/dlmalloc/dmalloc.cpp:5755` | `my_sbrk` | `0x00AE8590` | my_sbrk returns NULL on failure, which dlmalloc does not take for MORECORE_FAILURE |
| `src/volition/rfg/code/os/dlmalloc/dmalloc.cpp:5756` | `my_sbrk` | `0x00AE8590` | my_sbrk returns NULL on failure, which dlmalloc does not recognise as MORECORE_FAILURE |

### rfg/pathfinding

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:2261` | `create_compound_repulsors` | `0x00AFA570` | create_compound_repulsors merges repulsors without checking the 8 object limit of a compound repulsor |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:3958` | `bb_pf_get_closest_nav_cell` | `0x00B0F140` | bb_pf_get_closest_nav_cell compares a squared edge distance with a signed height difference |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:4279` | `bb_pf_calculate_turn_to_target` | `0x00B15DF0` | bb_pf_calculate_turn_to_target tests repulsor edges against the steering saved before any reflection *Note: Only this caller of get_nearby_repulsor_edges reflects* |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:4947` | `bb_pf_destroy_hole_navpoint` | `0x00B03970` | bb_pf_destroy_hole_navpoint walks forward while removal swaps the last link in and shrinks the count, so half the links stay *Note: Inlined in handle_hole_in_interior_wall; remove_navpoint_link swaps with the last link rather than shifting* |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:5248` | `check_nav_links_against_destruction_event` | `0x00B04190` | check_nav_links_against_destruction_event grows a copy of the box by the walkway radius, then tests the ungrown box |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:5944` | `rfg_pf_vehicle_get_path` | `0x00B1BF60` | rfg_pf_vehicle_get_path splices an out of chunk segment into the total path twice |
| `src/volition/rfg/code/pathfinding/nav_cell.cpp:358` | `find_longest_line_of_sight_point_internal` | `0x00B11BE0` | find_longest_line_of_sight_point_internal tests the intersection from the previous pass because it is copied before this pass writes it |
| `src/volition/rfg/code/pathfinding/nav_cell.cpp:1148` | `nav_cell_walk_line_of_sight` | `0x00B12680` | nav_cell_test_line_of_sight steps its adjacency test by sizeof(nav_cell) from the first adjacent cell instead of indexing adjacent_cells |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:1579` | `combine_compound_hull_2d_internal` | `0x00B0A980` | combine_compound_hull_2d_internal renames navpoint and action node block refs but never guard node block refs |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:2911` | `ncd_add_impassable_nav_cells_for_compound_hull` | `0x00B07F40` | ncd_add_impassable_nav_cells_for_compound_hull loops over three vertices but only ever tests the first |

### rfg/scripting

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:2737` | `Function_callback_squads_are_alive` | `0x00B271F0` | squads_are_alive asks simulated() and drops the answer, so every living squad counts |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:2769` | `Function_callback_human_is_assigned_custom_behavior` | `0x00B272A0` | human_is_assigned_custom_behavior does not check the ai pointer for null |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:3221` | `Event_callback_player_used_action_node` | `0x00B21280` | player_used_action_node does not check the local player for null Also: player_started_action_node does not check the local player for null |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:3578` | `Action_callback_raid_register_object` | `0x00B21890` | raid_register_object tests the count after registering, so 0x81 entries can go in |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4095` | `Action_callback_activity_spawn` | `0x00B27BD0` | activity_spawn reinstates activities for any object, not only a raid node |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4189` | `Action_callback_npc_give_weapon_and_equip` | `0x00B27F40` | npc_give_weapon_and_equip does not check the npc's ai for null |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4383` | `Action_callback_set_npc_hitpoints` | `0x00B286D0` | set_npc_hitpoints ends the list at the first handle with no object instead of skipping it |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4573` | `Action_callback_custom_action_wounded_follow` | `0x00B28C50` | custom_action_wounded_follow dereferences null when the spawn node has no live squad |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4704` | `Action_callback_spawn_squad_with_icons` | `0x00B29210` | Icon callbacks test the type index against 4 with 3 types, so index 3 reads the first minimap element |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4806` | `Action_callback_squad_despawn_object` | `0x00B29520` | squad_despawn_object destroys the squad's orders twice |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:5141` | `Action_callback_play_two_person_animation` | `0x00B2A590` | play_two_person_animation does not check the local player for null |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:5470` | `Action_callback_human_teleport_into_vehicle` | `0x00B2B440` | human_teleport_into_vehicle asks the seat count without checking the vehicle for null |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:5531` | `Action_callback_player_squad_remove_all` | `0x00B2B640` | player_squad_remove_all dereferences null when the player has no ally squad |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:5832` | `Action_callback_set_extra_mass` | `0x00B2C260` | set_extra_mass turns any value of at most 1 into 1 |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:6048` | `Action_callback_change_flyer_patrol_height` | `0x00B2C8D0` | change_flyer_patrol_height moves the squad object itself when the squad has no vehicle |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:6218` | `Action_callback_mission_hostage_icons_show` | `0x00B2D020` | mission_hostage_icons_show writes the list into a 3 handle stack buffer with no bound Also: mission_hostage_icons_show counts entries that failed, leaving their slots unset |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:6622` | `Action_callback_fog_of_war_show_important_buildings` | `0x00B32710` | fog_of_war_show_important_buildings leaves out movers beyond z 2070, whatever the district |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:82` | `objective_highlight_do_frame_handshake` | `0x00B33D50` | Highlight_glow_disable is only ever read, nothing writes it, so the switch has no effect |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:91` | `glow_set_rl_outline` | `0x00B2DA70` | glow_set_rl_outline does not check a human's char_inst for null |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:198` | `clear_glow_effect` | `0x00B33710` | clear_glow_effect does not check a human's char_inst for null |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:767` | `objective_highlight_ground_marker_add` | `0x00B2E1E0` | objective_highlight_ground_marker_add never reads stick_to_ground; vehicles always go to the ground |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:864` | `objective_highlight_do_frame` | `0x00B3E550` | objective_highlight_do_frame reads the previous entry's data when a squad's removal took the next entry out of the table |
| `src/volition/rfg/code/scripting/public_address.cpp:251` | `public_address_find_speaker_index_range` | `0x00B34210` | public_address_find_speaker_index_range steps down before it looks, so it can take in the last speaker of the zone before |
| `src/volition/rfg/code/scripting/public_address.cpp:547` | `public_address_group_play_queued_lines` | `0x00B3ED50` | Queued lines started while the kiosk holds the persona are re-queued on the same list, which the end of the function empties |
| `src/volition/rfg/code/scripting/target_outline.cpp:28` | `target_outline_set_local_player_locking_int` | `0x00B242B0` | Local_player_locking_int is written by its setter and read by nothing |
| `src/volition/rfg/code/scripting/target_outline.cpp:96` | `outline_specific_object` | `0x00B353E0` | A negative size_override keeps the current color, so the color worked out for the outline goes unused |

### rfg/spawn

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:118` | `find_next_navpoint_direction` | `0x00B4ED2A` | find_next_navpoint_direction's MIN_DOT is never written, so it is 0 and means 90 degrees, not 45 |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:331` | `max_vehicle_count` | `0x00B41131` | max_vehicle_count reads the NPC table for a mission's vehicle lock |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:539` | `player_bounding_region::inside_min_radius_npc` | `0x00B41BF0` | inside_min_radius_npc ends on the squared maximum distance, not the minimum |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:580` | `player_bounding_region::inside_min_radius_vehicle` | `0x00B41E70` | inside_min_radius_vehicle ends on the squared maximum distance, not the minimum |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:995` | `select_current_guerrillas` | `0x00B4C650` | One common guerrilla alone is not selected by select_current_guerrillas, it takes two |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:1411` | `do_frame_despawn_npc_critical` | `0x00B4DC90` | do_frame_despawn_npc_critical skips NPCs on foot while the player is on foot, as both vehicle handles are none |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:1424` | `do_frame_despawn_npc_critical` | `0x00B4DC90` | do_frame_despawn_npc_critical asks for the seat of an NPC it has just destroyed |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:1744` | `nearest_local_ped_node_network` | `0x00B59550` | nearest_local_ped_node_network adds the network once per node in the area, so the list repeats it |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2088` | `spawn_npc_navpoint` | `0x00B5EDBE` | Every ambient NPC gets behaviour group 2: spawn_npc_navpoint searches the regions then stores a constant, ignoring ambient_behavior_id |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2200` | `select_despawning_npc` | `0x00B47850` | select_despawning_npc compares 20 with a squared distance, so its limit is 4.5 m, not 20 |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2299` | `do_frame_spawn_npc` | `0x00B5F640` | do_frame_spawn_npc needs two spawn nodes in the list to use one |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2337` | `near_other_ambient_vehicle` | `0x00B4E090` | near_other_ambient_vehicle compares 20 with a squared distance, so its limit is 4.5 m |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2639` | `pick_ambient_vehicle_for_delete` | `0x00B47970` | pick_ambient_vehicle_for_delete never sets the position it transforms, so its front test means nothing *Note: Port starts from zero, rfg.exe from stack garbage.* |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2640` | `pick_ambient_vehicle_for_delete` | `0x00B47A75` | pick_ambient_vehicle_for_delete's behind-the-camera branch compares a distance with itself, so it is never taken |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:2943` | `ped_node_get_network_nodes` | `0x00B5A0B0` | ped_node_get_network_nodes empties the list for each network, so only the last network's nodes come back |
| `src/volition/rfg/code/spawn/ambient_spawn_manager.cpp:3381` | `build_new_waypoints_despawn` | `0x00B4F723` | build_new_waypoints_despawn never moves off the root, so it lists the root's links, maybe repeated |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:875` | `try_road_stream_pos` | `0x00B56D9F` | move_streaming_vehicle_road computes a final orientation along the road and moves with the original one |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:1229` | `building_child_list::reset` | `0x00B481E0` | building_child_list::reset does not clear action_node_list |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:1432` | `spawn_npc` | `0x00B48330` | spawn_npc reads the handle of a failed create without a null check |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:1545` | `spawn_npcs_in_nav_cell` | `0x00B5ADE0` | spawn_npcs_in_nav_cell keeps the first untried npc through all 49 offsets, so later offsets only overwrite its spawn_pos |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:2017` | `do_npc_spawn_building_spawn_node` | `0x00B5BE10` | do_npc_spawn_building_spawn_node counts up while its node list shrinks, so it can stop with nodes left |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:2201` | `cover_node_usable` | `0x00B5C580` | cover_node_usable requires the disabled timer valid and elapsed, so a node whose timer was never set is refused |

### rfg/traffic

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/traffic/traffic.cpp:45` | `Ignore_intersect` | `0x0374BFDC` | Ignore_intersect is never written, so it stays false |
| `src/volition/rfg/code/traffic/traffic.cpp:188` | `traffic_get_lanes_in_aabb_crude` | `0x00B60820` | traffic_get_lanes_in_aabb_crude drops its exclude filter |
| `src/volition/rfg/code/traffic/traffic.cpp:345` | `traffic_load_master` | `0x00B60C30` | traffic_load_master does not fix up yield_to_lanes or approx_points Also: traffic_load_master counts vehicle_list from the lane array |

### rfg/vehicle

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/vehicle/automobile.cpp:84` | `automobile_do_frame_postsim` | `0x03770028` | Powerslide_other_axle_friction_amount is never set, so a powerslide leaves other axles' friction alone |
| `src/volition/rfg/code/vehicle/automobile.cpp:984` | `automobile_handle_human_run_over` | `0x00B9BBF0` | several vehicle functions read through a null object when a handle is not the right type *Note: representative site; others in automobile.cpp, walker.cpp, vehicle_component.cpp commented likewise* |
| `src/volition/rfg/code/vehicle/vehicle_audio.cpp:279` | `vehicle_audio_engine_update` | `0x00B967B0` | automobile gear shift sounds store their play id in engine_peel, so gear_shift never plays |
| `src/volition/rfg/code/vehicle/vehicle_audio.cpp:397` | `automobile_honk_horn` | `0x00B75F50` | automobile_honk_horn reads through a null object for a handle that is not an automobile *Note: comment is on the first line of the function* |
| `src/volition/rfg/code/vehicle/vehicle_audio.cpp:411` | `automobile_release_horn` | `0x00B760C0` | automobile_release_horn reads through a null object for a handle that is not an automobile *Note: comment is on the first line of the function* |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:3611` | `vehicle_is_usable` | `0x00B8C160` | vehicle_is_usable reads NPC fields of any occupant, the player included |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:3679` | `vehicle_flip_upside_down` | `0x00B79340` | vehicle_flip_upside_down sets a world height of 2, not a lift of 2 |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:4187` | `vehicle_freeze` | `0x00B8CB30` | vehicle_freeze reads the automobile's data without checking it is loaded *Note: the vehicle itself is checked; the unchecked null is the loaded data* |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:4295` | `vehicle_get_number_of_components` | `0x00B799A0` | vehicle_get_number_of_components reads through a null vehicle for a bad handle *Note: comment is on the first line of the function* |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:4340` | `vehicle_set_hit_points` | `0x00B79A70` | vehicle_set_hit_points writes through a null vehicle for a bad handle |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:4521` | `vehicle_disable_collisions` | `0x00B79C40` | vehicle_disable_collisions saves a component's filter into the handle parameter, so it is lost |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:5074` | `vehicle_do_frame_postsim` | `0x00BC0950` | vehicle_do_frame_postsim frees the spline update for a sleeping body without clearing the pointer, so update_render frees it again |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:5732` | `vehicle_set_controls_when_abandoned` | `0x00B8D4F0` | vehicle_set_controls_when_abandoned reads through a null object for a bad handle *Note: comment is the next line* |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:359` | `vehicle_component::free_rigid_body_and_constraints` | `0x00B8DDE0` | free_rigid_body_and_constraints reads before the first component for an out of range alt body |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:422` | `vehicle_component::apply_damage` | `0x00BBFA90` | vehicle_component::apply_damage's 0xFFFF bound is the wrong way round, so tough parts keep the low 16 bits of the larger value |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:641` | `vehicle_component::update_animatable_door` | `0x00BAA6E0` | update_animatable_door does not check vi_info for null |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:1620` | `vehicle_detach_tread` | `0x00B9EDD0` | a failed create_object is dereferenced after spawning tread debris *Note: inlined in vehicle::component_detach* |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:1979` | `vehicle_get_component_hit_points` | `0x00B7BF00` | vehicle_get_component_hit_points reads through a null vehicle for a bad handle *Note: comment is on the first line of the function* |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:1991` | `vehicle_set_component_hit_points` | `0x00B7BF40` | vehicle_set_component_hit_points has the 0xFFFF bound the wrong way round |
| `src/volition/rfg/code/vehicle/vehicle_effects.cpp:73` | `automobile_tire_track_infos_read_table` | `0x00B663F0` | automobile_tire_track_infos_read_table leaves the table open if the list ends early |
| `src/volition/rfg/code/vehicle/vehicle_effects.cpp:221` | `automobile_tire_track_decal_adjust` | `0x00B7C8D0` | a track leaving an object searches for the fitting decal, then uses the longest |
| `src/volition/rfg/code/vehicle/vehicle_effects.cpp:410` | `automobile_play_collision_effects` | `0x00B7CD10` | the scrape effect plays with the vehicle's orientation and the matrix built for it goes unused |
| `src/volition/rfg/code/vehicle/vehicle_flyer.cpp:789` | `flyer::flight_compute_physics` | `0x00B7E800` | flight_compute_physics uses a whole velocity as an acceleration when near its target |
| `src/volition/rfg/code/vehicle/vehicle_flyer.cpp:899` | `flyer::flight_compute_physics` | `0x00B7E800` | flight_compute_physics tilts with the off-axis spin rather than against it |
| `src/volition/rfg/code/vehicle/vehicle_flyer.cpp:1201` | `flyer_blow_up` | `0x00B8F390` | flyer_blow_up removes actions from an array that shrinks as it walks it, leaving every other action |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:552` | `vi_door_open_action` | `0x00BA1520` | vi_door_open_action leaves its slot at 0 when the human's door index was already right |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:1301` | `vi_human_pathfind_to_interior` | `0x00BABE60` | vi_human_pathfind_to_interior reads nodes[-1] for the search's first node |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:1344` | `vi_human_pathfind_to_interior` | `0x00BABE60` | a route over 6 nodes overruns path_nodes in vi_human_pathfind_to_interior |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:1895` | `set_enter_pathfind` | `0x00BB7A10` | set_enter_pathfind walks all 44 waypoint usage slots, reading past a smaller vehicle's waypoint array |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:2344` | `process_enter_path_to_door` | `0x00BBBE40` | process_enter_path_to_door's two collision branches rewrite the value they just tested |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:2491` | `set_enter_teleport_wait` | `0x00BB4D90` | set_enter_teleport_wait passes an unfilled in_use array to vi_register_node |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:2519` | `process_enter_teleport_wait` | `0x00BB4FF0` | process_enter_teleport_wait carries on with a stale -1 waypoint after re-pathing |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:2734` | `set_shared_wait_waypoint_clear` | `0x00BA28C0` | path_nodes[...] of -1 index the waypoints before their array *Note: also process_shared_transition 0x00BBE3F0* |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:2830` | `process_shared_wait_waypoint_clear` | `0x00BBDF20` | process_shared_wait_waypoint_clear tests human_in_protected_vehicle_pos and does the same on both branches |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:3836` | `vi_get_exit_teleport_point` | `0x00B90290` | vi_get_exit_teleport_point turns to the new place only when it is within 0.1 m of the human |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:3990` | `set_exit_complete` | `0x00BB23A0` | set_exit_complete tests exit flags remove_human just cleared, so its crouch never runs |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:4346` | `vi_process_anim_state` | `0x00BBF580` | a handle that stops being a human is read through null in the anim state processing *Note: similar sites at 0x00BB4B00, 0x00B81200, 0x00BA1880, 0x00BB44D0* |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:5415` | `vehicle_interaction_highlight_entry_points` | `0x00B68830` | vehicle_interaction_highlight_entry_points starts one past the last waypoint |
| `src/volition/rfg/code/vehicle/vehicle_interaction_info.cpp:90` | `vehicle_interaction_parse_table` | `0x00B81ED0` | running out of interaction pool returns without closing the table |
| `src/volition/rfg/code/vehicle/vehicle_interaction_info.cpp:311` | `vehicle_interaction_info::get_waypoint_world_pos_orient` | `0x00B82860` | get_waypoint_world_pos_orient's range check uses && so it is unreachable |
| `src/volition/rfg/code/vehicle/vehicle_system.cpp:59` | `vehicle_force_corpse_removal` | `0x00B91970` | vehicle_force_corpse_removal fills its 32 entry arrays with no bound |
| `src/volition/rfg/code/vehicle/vehicle_system.cpp:724` | `vehicle_lookup_by_name_partial_match` | `0x00B923F0` | the partial name lookup lowers a copy of the searched name but searches with the original |
| `src/volition/rfg/code/vehicle/vehicle_system.cpp:765` | `vehicle_lookup_by_display_name_partial_match` | `0x00B92580` | the partial display name lookup never lowers the searched name, only the candidate |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:448` | `vehicle_parse_vehicle` | `0x00B928F0` | groups read Wheel_Spring_Pow while vehicles read Wheel_Spring_Power |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:741` | `vehicle_parse_foleys` | `0x00B6B5D0` | a vehicle without a Foley section takes the default engine, not its group's |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:764` | `vehicle_parse_foleys` | `0x00B6B5D0` | the vehicle's wave bank is looked up before its engine is read, from a stale engine id |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:1233` | `vehicle_parse_rig` | `0x00B6CD50` | an unknown footstep list returns before writing the footstep_effects field |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:1400` | `vehicle_parse_vehicle` | `0x00B928F0` | an unknown vehicle group gives index -1, read in front of the group array |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:1418` | `vehicle_parse_vehicle` | `0x00B928F0` | the abandon fraction is clamped to 100 after the division by 100 |
| `src/volition/rfg/code/vehicle/vehicle_table_parser_shared.cpp:210` | `build_vehicle_containers_shared` | `0x00BA30C0` | a missing animation table ends the whole container build without assasm_finish |
| `src/volition/rfg/code/vehicle/walker.cpp:246` | `walker_move_component` | `0x00B6D420` | walker_move_component when teleporting overwrites its position delta with the current position |
| `src/volition/rfg/code/vehicle/walker.cpp:1810` | `walker_blow_up` | `0x00B86530` | walker_blow_up removes actions from an array that shrinks as it walks it, leaving every other action |
| `src/volition/rfg/code/vehicle/walker.cpp:2197` | `wik_joint::get_ik_pos` | `0x00B996F0` | wik_joint::hit_point_valid is never set true, so get_ik_pos's cached branch never runs |

### rfg/vehicle_ai

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/vehicle_ai/intersection_controller.cpp:86` | `ixc_get_upcoming_point` | `0x00BCC0E0` | ixc_get_upcoming_point reads through a null road on a stale handle |
| `src/volition/rfg/code/vehicle_ai/intersection_controller.cpp:237` | `ixc_vehicle_can_be_queue_controlled` | `0x00BC2FF0` | ixc_vehicle_can_be_queue_controlled treats any driver that is not a player as an NPC |
| `src/volition/rfg/code/vehicle_ai/intersection_controller.cpp:350` | `ixc_vehicle_sort` | `0x00BC30B0` | ixc_vehicle_sort subtracts 25 from the entry at the lower address, so its answer depends on where qsort moved them |
| `src/volition/rfg/code/vehicle_ai/lane_pathfind.cpp:241` | `create_lane_path` | `0x00BCCF2B` | create_lane_path writes through null on an empty free pool |
| `src/volition/rfg/code/vehicle_ai/lane_pathfind.cpp:322` | `query_side` | `0x00BD4427` | query_side tests the GPS flag of the link after the forward links, whichever side it is changing to |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:254` | `vehicle_ai_add_route_segment` | `0x00BCD500` | vehicle_ai_add_route_segment drops the oldest segment without adjusting center_info and bumper_info indices |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:1085` | `process_vehicle_steering` | `0x00BCFF70` | process_vehicle_steering uses last frame's goal position for the terrain height of a flyer's goal |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:2081` | `vehicle_ai_begin_script_ambient_pathfind_to` | `0x00BD5350` | vehicle_ai_begin_script_* only report whether a path exists and leak the path they created |
| `src/volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp:576` | `vehicle_get_navmesh_path_internal` | `0x00BCF0F0` | vehicle_get_navmesh_path_internal reads through a null last node if the spline part made no path |
| `src/volition/rfg/code/vehicle_ai/vehicle_path_request.cpp:52` | `vehicle_navmesh_path_add_request` | `0x00BC7520` | A new navmesh path request takes time 0 rather than its offset, so a requeued request restarts its timeout |
| `src/volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp:117` | `get_repulsor_collector_bbox` | `0x00BC7672` | get_repulsor_collector_bbox negates the caller's unset bmin.x for a flyer with no body |
| `src/volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp:501` | `maybe_ignore_human` | `0x00BCAC09` | The road's width at the human is fetched and discarded in the human obstacle case *Note: Port drops the pure get_width call.* |
| `src/volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp:761` | `maybe_ignore_vehicle` | `0x00BCB560` | An ambient vehicle told to pull off reads other_driver with no null check |
| `src/volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp:961` | `finish_repulsion_vector` | `0x00BD1A40` | calculate_vehicle_repulsion_vector's result is dist < 0 with dist from mag_and_normalize, so always false, and the caller ignores it |
| `src/volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp:1209` | `update_active_repulsor_list` | `0x00BC8910` | update_active_repulsor_list searches only the kept entries, so a handle listed twice is added twice |

### rfg/video_player

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/video_player/video_player.cpp:10` | `video_player_play` | `0x00BD81C0` | video_player_play ignores its volume, surface_buffer_pool and contains_music arguments |
| `src/volition/rfg/code/video_player/video_player.cpp:641` | `video_player_play` | `0x00BD81C0` | video_player_play returns without freeing the tracker when the file is missing |
| `src/volition/rfg/code/video_player/video_player.cpp:908` | `video_player_process_all` | `0x00BD86D0` | the second video in the list does not throttle the disc (count != 2) |
| `src/volition/rfg/code/video_player/video_player.cpp:1067` | `video_player_system_shutdown` | `0x00BD7F30` | video_player_system_shutdown closes the movies without freeing the trackers |

### rfg/weapons

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/rfg/code/weapons/arc_welder.cpp:78` | `create_electrocution_effect` | `0x00BE47C0` | create_electrocution_effect looks the hit object up again by its own handle. |
| `src/volition/rfg/code/weapons/arc_welder.cpp:289` | `arc_welder_pre_collect_cb` | `0x00BDB600` | arc_welder_pre_collect_cb fills an unused fire info and draws a random tag it never uses. |
| `src/volition/rfg/code/weapons/arc_welder.cpp:310` | `arc_welder_collect_cb` | `0x00BE9020` | arc_welder_collect_cb uses the owner's weapon before its null check. |
| `src/volition/rfg/code/weapons/arc_welder.cpp:439` | `arc_welder_test` | `0x00BE9180` | arc_welder_test computes a tag position for a human and does not use it. |
| `src/volition/rfg/code/weapons/arc_welder.cpp:469` | `arc_welder_test` | `0x00BE9180` | A zone hit without a conductive material clears the cache, and the straight shot can still take that zero-rated hit. |
| `src/volition/rfg/code/weapons/weapons.cpp:3120` | `weapon_create_curved_tracer` | `0x00BE3CC0` | weapon_create_curved_tracer dereferences a null vfx_effect when the visual is not loaded. |
| `src/volition/rfg/code/weapons/weapons_table_parser.cpp:369` | `parse_weapon_info` | `0x00BDDD60` | Attachment_Point is copied into the 32-byte attachment_point with an unbounded copy. |
| `src/volition/rfg/code/weapons/weapons_table_parser.cpp:437` | `parse_weapon_info` | `0x00BDDD60` | Up to 17 NPC fire sound cues are written into the 16 entries of npc_fire_sounds. |

### vfxlib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/vfxlib/code/vfx_effect.cpp:165` | `vfx_effect::create` | `0x00CBA670` | vfx_effect::create dereferences a null effect when all 512 effects are in use |
| `src/volition/vfxlib/code/vfx_effect.cpp:332` | `vfx_effect::init` | `0x00CB9AC0` | When the filter instance pool runs out, init dereferences the null instance |
| `src/volition/vfxlib/code/vfx_effect.cpp:372` | `vfx_effect::init` | `0x00CB9AC0` | init gives a renderable filter instance the index in the affected-mesh list as its mesh index |
| `src/volition/vfxlib/code/vfx_effect.cpp:426` | `vfx_effect::init` | `0x00CB9AC0` | Emitter parents are matched by instance order, off after an emit-node emitter that is not instanced |
| `src/volition/vfxlib/code/vfx_effect.cpp:542` | `vfx_effect::release` | `0x00CB1BC0` | release walks the coronas of the renderable particle group it has already released |
| `src/volition/vfxlib/code/vfx_effect.cpp:1350` | `vfx_effect::get_variable_count` | `0x007420B0` | get_variable_count always returns 1 |
| `src/volition/vfxlib/code/vfx_particle_system_emitter.inl:44` | `vfx_particle_system_emitter::get_local_rotation` | `0x00C9D080` | get_local_rotation's single-key copy falls through and is overwritten by the interpolation |
| `src/volition/vfxlib/code/vfx_particle_system_emitter.inl:1294` | `vfx_particle_system_emitter::set_size_mida_variance` | `0x00C9CE40` | set_size_mida_variance sets the mid-A size key count instead of the variance key count |

### vlib

| Location | Function | rfg.exe | Description |
|---|---|---|---|
| `src/volition/vlib/code/bmpman/bmpman.cpp:704` | `calc_bitmap_sizes` | `0x005B5270` | calc_bitmap_sizes never steps past a bitmap entry with num_frames 0, so the loop does not end |
| `src/volition/vlib/code/bmpman/bmpman.cpp:941` | `bm_save_peg` | `0x005CE440` | bm_save_peg with one file checks, writes and closes the already closed CPU file again as the GPU file |
| `src/volition/vlib/code/bmpman/bmpman.cpp:972` | `bm_save_peg` | `0x005CA100` | bm_save_peg writes entries as stack copies, so saved name/next/prev offsets are relative to the copy |
| `src/volition/vlib/code/bmpman/bmpman.cpp:974` | `bm_save_peg` | `0x005CA100` | bm_save_peg never steps past an entry with num_frames 0 when writing the names |
| `src/volition/vlib/code/bmpman/peg_assemble.cpp:13` | `copy_bitmap_data_with_interleave` | `0x005CE540` | copy_bitmap_data_with_interleave steps one entry past an interleaved bitmap instead of by num_frames |
| `src/volition/vlib/code/bmpman/peg_assemble.cpp:127` | `copy_bitmap_data_with_interleave` | `0x005CE540` | copy_bitmap_data_with_interleave does not check the .cvbm_pc load; a failed load reads entries at address 0x18 |
| `src/volition/vlib/code/math/ix.cpp:1136` | `ix2d_linesegment_linesegment` | `0x005736A0` | ix2d_linesegment_linesegment dereferences ix without the null check its callee makes |
| `src/volition/vlib/code/os/os.cpp:307` | `os_hook_message` | `0x0058B460` | os_hook_message has no bound check against the 32 hook slots |
| `src/volition/vlib/code/os/os.cpp:319` | `os_unhook_message` | `0x0058B480` | os_unhook_message moves a count of hooks as bytes instead of 4-byte entries |
| `src/volition/vlib/code/util/rule_file.cpp:34` | `slw_get_textures` | `0x005CD770` | slw_get_textures leaves its temp pool mark in place on early returns |
| `src/volition/vlib/code/util/vfont.cpp:6` | `font_get_char_width_template` | `0x005C0E30` | font_get_char_width's kerning search compares code points in the loop but takes a pair by glyph data index |
| `src/volition/vlib/code/util/vfont.cpp:12` | `font_split_str_template` | `0x005B4270` | font_split_str_template copies each line into a 512-byte buffer without a bound |

## Listed in a doc, not a bug in rfg.exe

The doc's description did not hold up against the binary. Where the port itself behaved that way, the port has been fixed to match `rfg.exe`; the note says so.

| Location | Function | rfg.exe | Description | Note |
|---|---|---|---|---|
| `src/volition/interface_lib/code/scripting/vint_lua_actions.cpp:1185` | `vint_object_find` | `0x0046AE55` | vint_object_find with a nil parent reads the document handle as the parent | Port fixed to match rfg.exe. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:269` | `StructArrayImplementation::renameMember` | `0x00F4A310` | StructArrayImplementation::renameMember scans past the end of the array for an unknown name (same wrong comment as the removals) | Port fixed to match rfg.exe. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:695` | `hkDataObjectDict::removeMember` | `0x00F4D1E0` | hkDataObjectDict::removeMember scans past the end of the array for a name that is not set | Port fixed to match rfg.exe. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:719` | `hkDataObjectDict::renameMember` | `0x00F49650` | hkDataObjectDict::renameMember scans past the end of the array for a name that is not set (same wrong comment as the removals) | Port fixed to match rfg.exe. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:2273` | `StructArrayImplementation::removeMember` | `0x00F4BDC0` | StructArrayImplementation::removeMember scans past the end of the array for an unknown name | Port fixed to match rfg.exe. |
| `src/volition/rfg/code/hud/hud_icon.cpp:560` | `hud_icon_get_bitmap_id` | `0x00844880` | HI_INVALID (-1) passes the bitmap id check and reads before the table | Port fixed to match rfg.exe. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:1397` | `hud_ui_multiplayer_get_doc_handle` | `0x008980A0` | HMD_NONE (-1) passes the doc handle check and reads before the table | Port fixed to match rfg.exe. |
| `src/volition/rfg/code/objects/object_cover_node.cpp:428` | `cover_node::locate_node` | `0x00A3A683` | locate_node leaves the stored point uninitialized when nothing was searched | Port fixed to match rfg.exe. |
| `src/volition/rfg/code/objects/object_cover_node.cpp:449` | `cover_node::locate_node` | `0x00A3A625` | locate_node overwrites the point stored for a node in a cell | Port fixed to match rfg.exe. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:448` | `roadblock_data_type::load` | `0x00A92D70` | roadblock_data_type::load can fail with squads still allocated | Every false return has an empty list: early returns follow reset, squads that fail are freed, the last return is num > 0 (asm 0x00A92E6A). Port matches. |
| `src/volition/rfg/code/vehicle_ai/lane_pathfind.cpp:229` | `are_lanes_adjacent` | `0x00BCCE96` | are_lanes_adjacent's right walk tests the starting lane's flags at every step | Port fixed to match rfg.exe. |

## Not in src

The doc lists these, but the code is not in `src`.

| Location | Function | rfg.exe | Description | Note |
|---|---|---|---|---|
| - | `object_effect::render_step_debug` | `0x005D7190` | object_effect::render_step_debug dereferences the nullptr from rl_getDebugRenderer (dead debug draw) | Declared in object_effect.h, no definition in src/. |

## Not settled

The comparison could not decide these.

| Location | Function | rfg.exe | Description | Note |
|---|---|---|---|---|
| `src/volition/render_lib/code/rl_shader_fixup.h:4` | `rl_shader_fixup` | `-` | rfg-decal-bloodsplat vertex shaders read the unorm NORMAL without the mad 2,-1 decode, so decals facing -x/-y/-z vanish | Shader blobs are game data, not code in rfg.exe, so Ghidra cannot confirm it. The port's DXBC patch has a deviation comment. |
| `src/volition/render_lib/code/rl_shader_fixup.h:15` | `rl_shader_fixup` | `-` | rfg-skybox_s computes asin via sqrt(mad(-y,0.1,1)); with FMA at y=10 that goes negative and the apex vertex becomes NaN | Shader blob is game data, not code in rfg.exe, so Ghidra cannot confirm it. The port's DXBC patch has a deviation comment. |
| `src/volition/rfg/code/effects/object_effect.cpp:516` | `object_effect::stream` | `0x0074D060` | The radial blur rank of a playing effect without blur info is whatever a register holds | rfg.exe keeps EDX left by effects_get_visual_effect_info, which does not set it; the port uses -0x7FFFFFFF. Doc marks it inferred. |

## Other bugs commented in the code

These come from the code comments and appear in no doc, so they were **not checked individually** against `rfg.exe`. The description is the comment's own text.

### Havok SDK

| Location | Comment |
|---|---|
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarrayutil.cpp:9` | _reduce passes the capacity word with its flag bits to bufRealloc as the old size. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.cpp:16` | GarbageCollect repeats its pass only while some list had free blocks and findGarbage returned a negative count, which it never does, so it runs once. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.cpp:18` | _newFreeList breaks into the debugger (int3) when the 41 free list slots are used up. Not reimplemented: walkMemory (needs hkMallocAllocator, S phase) fails, addToSnapshot (hkMemorySnapshot, S phase) records nothing. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.cpp:11` | Quit frees the first slab and the cached slab to the internal allocator, also when the cached slab came from the slab allocator. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/thread/hkthreadmemory.cpp:10` | InitLookupTables never sets m_row_to_size_lut[0]; getMemoryStatistics multiplies it by row 0's (always 0) count. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.cpp:284` | 0x00F69200 (:338). Original quirk kept: fillNativeMembers' result is not checked. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.cpp:474` | Members and null char pointers are written as comments. Original quirk kept: an enum name is printed as a format string. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp:697` | :749 (0x00F6C7C0). Original quirk kept: the member classes are looked up (findClass) and the result dropped. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.cpp:958` | Reference fix-up. Original quirk kept: a class definition that fails is skipped over, not reported. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.cpp:537` | AsStart() is not checked (the open node is always a start element here) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.cpp:476` | Both axles grip. Original quirk kept: the coupling always goes into axis 1's side slip, also from axis 1 itself, where it has no further reader. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.cpp:528` | The axle further over the limit goes first. Original quirk kept: both passes add the coupling to the side slip of the second axle, so the second axle's own impulse is added to it after it was used. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.cpp:247` | The index base does not move past a degenerate face, so the faces after it read its vertex indices |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.cpp:260` | A face without a normal gets the first remaining plane, which also stays in the remaining planes |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.cpp:226` | W gets the untruncated code. For a NaN direction the step converts to 0x80000000 (integer indefinite), so w is 0xBF000000 \| id (sign set) while the id (decoded above) is the low 16 bits. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.cpp:241` | The (local) extrusion is added to the world-space bounds untransformed. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.cpp:598` | X restarts from the cell before the x clip |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.cpp:98` | Allocator with size 0, an original quirk kept); when the schemas do not fit either, the rest goes to a temp allocation and a goto schema links the two parts; when not even the accumulators fit, everything is temp memory. A null solver ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.cpp:148` | Buffer's start: absolute with a preallocated buffer, an original quirk kept) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.cpp:118` | 0x00DDD290: original bug kept: a listener that is not there clears the slot before the array (index -1) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpcharactermotion.cpp:138` | The angular term uses normal x arm, as hkpSphereMotion (original quirk kept); the inverse virtual mass is the inverse mass only. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.cpp:139` | The angular term uses normal x arm, as hkpSphereMotion (original quirk kept). |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpspheremotion.cpp:132` | Unlike hkpBoxMotion, the angular term uses normal x arm (the binary's operand order), so the angular velocity counts with the opposite sign. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.cpp:49` | 0x00E04EE0: original bug kept: a listener that is not there removes index -1 (the whole array shifts down onto the slot before it) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.cpp:1201` | Entity's new constraints are sorted from its slave constraints on (an original quirk: the master ones added before the sort range stay unsorted), its new entities likewise. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.cpp:509` | The candidate index counts only the points not used yet, so a pick after the first used index names the point before it (the indices are rank among the unused points, used as array indices). |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.cpp:38` | Info and data). Original quirk kept: the capacity has no DONT_DEALLOCATE flag; such an object is never destroyed. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.cpp:402` | Property 0 is read before the copy, so the other properties are lost (and property 0 keeps the local's old value) when the query was not local yet |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/simulation/default/hkpvehiclesimulationbody.inl:252` | Reads the default damper's fields whatever the damper's class is. |

### interface_lib

| Location | Comment |
|---|---|
| `src/volition/interface_lib/code/bitmap_sheets.cpp:208` | The temp pool is not restored to its mark when the table is missing. |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.cpp:179` | An unknown handle goes on with no bitmap (get_dimensions reads through null). |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.cpp:281` | The bogus bitmap goes to set_image as an int, which set_image rejects. |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp:230` | The temp pool is not restored |
| `src/volition/interface_lib/code/objects/noninteractive/vint_element_text.cpp:323` | A NUL is skipped like white space here |
| `src/volition/interface_lib/code/objects/vint_object_base.cpp:1231` | The value is taken by value, so the caller never sees it. |
| `src/volition/interface_lib/code/objects/vint_object_base.cpp:1242` | The value is taken by value (and copied again), so the caller never sees it. |
| `src/volition/interface_lib/code/vint_document.h:81` | <float> 0x00467610, <unsigned int> 0x00467590 (.h:140). Original bug kept: the end test is inclusive, so a value that ends exactly at the end of the buffer cannot be read. |
| `src/volition/interface_lib/code/vint_external.cpp:432` | The colour is not initialized when there is no get_color callback (rfg.exe: whatever ECX held), the channels are divided as integers (so 0 or 1), and _alpha is not used. |
| `src/volition/interface_lib/code/vint_project.cpp:464` | Writes the mode after the last one instead of _index. |
| `src/volition/interface_lib/code/vint_project.cpp:650` | Once a mode of the language was taken, the language test compares the language with itself, so any nearer aspect ratio wins. |
| `src/volition/interface_lib/code/vint_project.cpp:669` | With no modes, mode -1 is read. |
| `src/volition/interface_lib/code/vint_project.cpp:788` | Copies up to 128 bytes into a 64-byte name (overflowing into the next one). |
| `src/volition/interface_lib/code/vint_string_processing.cpp:111` | At depth 15 the copy goes one entry past the stack (into format_depth's place, 0x1B0). |
| `src/volition/interface_lib/code/vint_variant.cpp:97` | The channels are not divided by 255 (operator=(color) does divide), so a colour constructed from a color holds 0..255 components. |
| `src/volition/interface_lib/code/vint_variant.cpp:904` | An unknown resource handle zeroes the handle without releasing the old reference. |
| `src/volition/interface_lib/code/vint_variant.cpp:1050` | The old callback's reference is not released. |

### keen/base

| Location | Comment |
|---|---|
| `src/library/modules/runtime/base/sources/global/formatparameterizedstring.cpp:11` | 'X' gives lower case digits like 'x' (both cases share one branch that clears bigHexDigits). |
| `src/library/modules/runtime/base/sources/global/formatparameterizedstring.cpp:12` | The hexadecimal prefix's two characters are added to the width. |
| `src/library/modules/runtime/base/sources/global/formatparameterizedstring.cpp:13` | The basis only applies to unsigned parameters; signed ones are always decimal. |
| `src/library/modules/runtime/base/sources/global/formatparameterizedstring.cpp:14` | A field without ')' prints "(field syntax error)" and the rest of the field is copied as text. |
| `src/library/modules/runtime/base/sources/global/formatstring.cpp:17` | A single l reads 64 bits, as ll does. |
| `src/library/modules/runtime/base/sources/global/formatstring.cpp:18` | An unknown conversion prints "[base] Invalid format string '%s'!" (the whole format) and writes '%'; the conversion character is then written as text. An unknown character after l or z prints "invalid input format" and is written as text ... |
| `src/library/modules/runtime/base/sources/global/formatstring.cpp:21` | There is no '*' width or precision, no h, no e/E/G/a. formatString(WriteStream*, ...) (0x00C05F70) is not ported: nothing in rfg.exe calls it. |
| `src/library/modules/runtime/base/sources/global/formattedstringwriter.cpp:11` | A left aligned integer gets its prefix only when it is not zero; right aligned it always gets it. |
| `src/library/modules/runtime/base/sources/global/formattedstringwriter.cpp:12` | A missing prefix is written as "[nullptr]". |
| `src/library/modules/runtime/base/sources/global/formattedstringwriter.cpp:13` | OutputFloatingPointFixed sizes the fraction from the unclamped precision but prints with the clamped one (0-100). |
| `src/library/modules/runtime/base/sources/global/formattedstringwriter.cpp:14` | OutputFixedPointInteger prints at most 6 fraction digits, and rounds a fraction of 1..0xFFE units up by one unit whatever decimalBitCount is. |
| `src/library/modules/runtime/base/sources/global/stringscanner.cpp:10` | The signed readers skip white space again after the sign ("- 5" reads as -5). |
| `src/library/modules/runtime/base/sources/global/stringscanner.cpp:11` | ReadApproximateFloatFromDecimalString accumulates in float with double intermediates (the original's x87 code) and accepts white space between the integer part and the '.'; it has no exponent and never overflows. |

### keen/gameframework

| Location | Comment |
|---|---|
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:350` | While there are more threads than allowed, the loop clears ~(i * 2) instead of a single thread's bit (i counting from 0), and it counts the bits of the mask before the first thread was removed. |

### keen/graphics

| Location | Comment |
|---|---|
| `src/library/modules/runtime/graphics/sources/global/postprocessingfxaa.cpp:55` | The sampler parameters reuse the rasterizer parameters' stack slot and the memset starts 4 bytes in, so mipLodBias keeps the bits of cullMode (2, a denormal float, effectively 0). |

### keen/input

| Location | Comment |
|---|---|
| `src/library/modules/runtime/input/sources/global/inputsystem.cpp:668` | Id 16 passes the check (one past the table). |

### keen/task

| Location | Comment |
|---|---|
| `src/library/modules/runtime/task/sources/global/taskqueue.cpp:8` | PushSyncTaskToQueue only prints that it is not implemented. |
| `src/library/modules/runtime/task/sources/global/taskqueue.cpp:9` | The copy slot ring has as many slots as the queue, so a task's copy stays valid until maxTaskCount further pushes. |
| `src/library/modules/runtime/task/sources/global/taskqueue.cpp:80` | The copy buffer and the event are not released (original leak). |
| `src/library/modules/runtime/task/sources/global/tasksystem.cpp:10` | Worker thread indices start at 1 (0 is the thread that waits for a queue). |
| `src/library/modules/runtime/task/sources/global/tasksystem.cpp:11` | Only the lowest 16 affinity bits get a worker. |

### lua_lib

| Location | Comment |
|---|---|
| `src/volition/lua_lib/code/lua_scripts.cpp:60` | Filename with ".lua" appended unless it already ends in it (case-insensitive). Original bug kept: a name shorter than four characters is compared from before its start. |
| `src/volition/lua_lib/code/lua_scripts.cpp:86` | Reads the script through cfile into temp_mempool and runs it (chunk name: the name as given). Original bug kept: when the pool cannot hold the file, the file stays open and the pool's mark is not restored. |
| `src/volition/lua_lib/code/lua_system_actions.cpp:198` | Returns its handle, or 0xFFFF once it has ended (or could not start). Original bug kept: called outside a script thread, the missing current thread is dereferenced. |
| `src/volition/lua_lib/code/lua_thread.cpp:109` | Thread uses. Original bug kept: without allocator callbacks the name copy writes through NULL. |
| `src/volition/lua_lib/code/lua_thread.cpp:342` | Marks the threads whose function name starts with prefix + "_". Original bug kept: the prefix is copied into a 64-byte buffer unchecked. |

### render_lib

| Location | Comment |
|---|---|
| `src/volition/render_lib/code/rl_hdr_compositor.cpp:372` | The final bloom stage's texture data is fetched and dropped (a call through its dynamic texture, which is null when bloom is off and sun shafts are not drawn: the level of detail must keep bloom on). |
| `src/volition/render_lib/code/rl_material_data_render.cpp:18` | The constants are copied to constant_block + const_loc floats with array_size * num_rows floats each, but the Specular_Power and Normal_Map_Height clamps index whole registers from 20 (const_loc - 0x14) and only for const_loc 20..51, the ... |
| `src/volition/render_lib/code/rl_material_data_render.cpp:22` | A constant whose name is not in the shader is copied to location -1 with size -4 bytes (a huge memcpy). The crunched materials of the game never have one. |
| `src/volition/render_lib/code/rl_particle_renderer.cpp:527` | The null geometry is dereferenced below. |
| `src/volition/render_lib/code/rl_terrain_renderer.cpp:19` | The glass variants (glassVariants) are part of the context but are never created. |
| `src/volition/render_lib/code/rl_terrain_renderer.cpp:20` | DestroyTerrainRenderContext does not destroy the roads bindings and constant buffers, the stitch alpha and roads vertex formats, or the substance and EOS glass variants, and the registered rl_shaders stay in the library. |

### rfg

| Location | Comment |
|---|---|
| `src/volition/rfg/code/game_render_terrain_render.cpp:64` | A road piece whose instance data lists stitched meshes reads the chunk name and mesh count through a null chunk. |
| `src/volition/rfg/code/game_render_terrain_render.cpp:65` | The mismatch message is formatted and dropped, and the mesh loop runs over the chunk's general objects, not the clamped count. |
| `src/volition/rfg/code/game_render_terrain_render.cpp:67` | When a blend skirt or its root mesh cannot be created, the one that was created is not released. |
| `src/volition/rfg/code/rfg.cpp:26` | Rfg_init_stage_2_done is run on its own thread by keen::rfg::Application while the render thread keeps drawing the boot screen; each block of systems is skipped once Game_boot_exit_flag is set, and the bail-out releases the preload pools. |
| `src/volition/rfg/code/rfg.cpp:29` | Localized_packfile_init returns false (no packfile added) for an audio language outside the table. |
| `src/volition/rfg/code/rfg.cpp:1226` | Rfg_init_stage_1_prepare registers the packfiles (init_packfiles) when Use_packfiles is set, which it sets itself; rfg_init_stage_1_done calls init_packfiles again only when Use_packfiles is clear (never in this build). |
| `src/volition/rfg/code/rfg.cpp:1228` | Every step is skipped once Game_boot_exit_flag is set; the bail-out releases the preload pools and stops the boot screen thread (boot_exit_cleanup; the original repeats the same code at each place). |
| `src/volition/rfg/code/rfg.cpp:1230` | Rfg_get_num_tracked_chunk_ids returns 12 for an array of three ids. |
| `src/volition/rfg/code/rfg.cpp:2318` | A failed allocation returns null (no std::bad_alloc). |
| `src/volition/rfg/code/rfg.cpp:2319` | The nothrow forms are not meant to be used: they print "What are you doing here?" and allocate or free nothing (a nothrow new returns null). Nothing in the port uses them. |
| `src/volition/rfg/code/rfg.cpp:2321` | Not replaced (newer compilers only; the CRT's stay and agree with the forms here): the sized deletes, which forward to operator delete(void*) / delete[](void*) below, and the std::align_val_t forms (alignment above 8), which use ... |

### rfg/ai

| Location | Comment |
|---|---|
| `src/volition/rfg/code/ai/ai_system.cpp:1048` | No check of the local player. |
| `src/volition/rfg/code/ai/ai_system.cpp:3017` | The subtype is read for any object type, not only for movers. |
| `src/volition/rfg/code/ai/percept.cpp:969` | Handle was just stored, so this only tells a real handle from the invalid one and the sticky flags stay for any object, not only for the same one. |
| `src/volition/rfg/code/ai/percept.cpp:3941` | The asm compares the mode with 4, ATTENTION_AIM_AT_BELIEF, which this frame never has, so ATTENTION_AIM_AT_OBJECT never aims through here. |
| `src/volition/rfg/code/ai/percept.cpp:5109` | It considers switching to the attacker (the player), not to the enemy. |
| `src/volition/rfg/code/ai/percept.cpp:6170` | Here neither direction is normalized before the 45 degree test. |
| `src/volition/rfg/code/ai/player_attack_limiter.cpp:229` | The loop runs to Last_npc_limit, so it also reads records past Num_player_attack_limiter_records, which can hold a handle from before they were dropped. |
| `src/volition/rfg/code/ai/search_system.cpp:509` | No null check of the searcher, which is alive here because it owns a search. |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:592` | No null check of the cast before the flags, human_is_dead answers for a null. |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:974` | Walking to the vehicle must not be most of the way there. Original quirk: for a goal position the box is empty and local_pos stays world. |
| `src/volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp:2787` | With neither side free rfg.exe sums an escape vector away from the others (40 m minus the distance to each) into dest and dest_point, then overwrites both with the right point just below; not kept as its result is never read. The inlined ... |

### rfg/camera

| Location | Comment |
|---|---|
| `src/volition/rfg/code/camera/camera_ms.cpp:31` | The end reads knot num_knots, one past the last one set. |

### rfg/code_effects

| Location | Comment |
|---|---|
| `src/volition/rfg/code/code_effects/explosion.cpp:18` | Explosion_create_internal checks the stream radius against the explosion's position box only for explosions flagged 0x10 (far explosions are skipped). |
| `src/volition/rfg/code/code_effects/explosion_shared.cpp:6` | Explosion_parse_entry stores up to 8 effect handles into m_effects[4] (the extra ones run into m_num_material_effects and m_material_effects, which are reset right after) and up to 9 material effects into arrays of 8. The <Effects> list is ... |
| `src/volition/rfg/code/code_effects/explosion_shared.cpp:9` | ConeAngle is clamped to 0..180 and half_angle_dot is the cosine of the whole angle. |
| `src/volition/rfg/code/code_effects/explosion_shared.cpp:10` | Explosion_parse_table stops at 79 explosions without closing the table. |

### rfg/control

| Location | Comment |
|---|---|
| `src/volition/rfg/code/control/control_map.cpp:1664` | Zero byte written as 0xFF), then 0x08 and the terminator. Original bug kept: a character starting one or two bytes before the end still writes its three bytes, up to two past the buffer. |
| `src/volition/rfg/code/control/control_map.cpp:2458` | Only the first origin is baked, the separator below never runs. |
| `src/volition/rfg/code/control/control_map.cpp:2706` | No check against the 5 mouse buttons of the table. |
| `src/volition/rfg/code/control/control_map.cpp:3046` | A text stays unset for a button number of 5 or more. |
| `src/volition/rfg/code/control/control_map.cpp:3176` | A text stays unset for a button number of 5 or more. |

### rfg/cutscene

| Location | Comment |
|---|---|
| `src/volition/rfg/code/cutscene/cutscene.cpp:599` | Starts and stops the actor's animations that are due. Original quirk kept: no null check of the actor's object. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:910` | An index past the end answers the entry one past the last. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:936` | No bound on the name, into a 32 byte field. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1465` | This leaves the launch flag set and Cutscene_rendered_this_frame as it was. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1509` | No bound on the name, into a 33 byte buffer. |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:561` | Despite the name, a missing element leaves the value as it was. rfg.exe inlines xml_find. |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:719` | A table with no navpoints passes an entry that was never filled. |
| `src/volition/rfg/code/cutscene/cutscene_timer.cpp:91` | The last shot ran out. Original quirk kept: m_frame_time is what was left of the shot, not of the frame. |

### rfg/destruction

| Location | Comment |
|---|---|
| `src/volition/rfg/code/destruction/rfg_dynamics.cpp:18` | Get_subpiece_index_from_object and rfg_dyn_filter_destruction read the general mover's subpiece index without checking that the object is a mover. |
| `src/volition/rfg/code/destruction/rfg_load_balance.cpp:14` | Rfgl_find_and_delete_* scan at most twice the number of objects to delete per call, from where the last call stopped. |
| `src/volition/rfg/code/destruction/rfg_materials.cpp:10` | Explode events name explosions; their handle is the explosion_info pointer. rfg_materials_system_init runs before explosion_system_init in the game, so those names are not found then. |
| `src/volition/rfg/code/destruction/rfg_materials.cpp:12` | Every other event names effects. Which kind an event child names is decided by the loop variable of the event type search, which the default-shape fill of a <default> block reuses (leaving it at 5): explode events with a <default> block ... |
| `src/volition/rfg/code/destruction/rfg_materials.cpp:15` | An effect material reads up to 11 gust effects into an array of 10 (the 11th overlaps the driving fields, which are set afterwards, and the next material); the percentages are normalised by their sum. |
| `src/volition/rfg/code/destruction/rfg_materials.cpp:17` | Rfg_materials_system_init dereferences Rfg_physical_material_default even when no "default" material exists. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:30` | Maybe_post_damage_sound compares percent_destroyed() (0..1) against 10, so only damage_amt >= 10 posts a sound. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:31` | Rfg_mover_regrow_begin requires percent_destroyed() >= REGROW_MIN_PERCENT_DESTROYED (2.0), which a damage percent (0..1) never reaches, so movers never regrow on stream unless the value is tweaked. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:33` | Create_deferred_ri resets creator_handle before passing it as the donor handle, so faders of the transferred pieces are looked up with owner 0xFFFFFFFF. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:35` | Rfg_mover_regrow_setup restores a link when the visited piece's own shape state is set, not the neighbour's. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:36` | Rfg_mover_regrow_begin clears rfg_subpiece_base_extra_data::h of anchored pieces in the shared base data. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:37` | Attach_index_is_valid accepts index == num_objects. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:38` | Set_move_type through find_anchor_points and add_to_world keeps the move type when the Havok handle is invalid. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:39` | Get_tag_local_coords returns false without touching the outputs when the chunk is missing. |
| `src/volition/rfg/code/destruction/rfg_mover.cpp:40` | Script_point_destroy dereferences the shape without checking it. |
| `src/volition/rfg/code/destruction/rfg_shapes.cpp:21` | Rfg_subpiece::restore_link swaps when index is greater than num_links, the opposite test of remove_link, so a link restored from below num_links is not moved back into place. |
| `src/volition/rfg/code/destruction/rfg_stress.cpp:26` | Without a "Default" entry in rfg_stress_controls.xtbl, Current_stress_control stays -1 and Stress_controls[-1] is read. |
| `src/volition/rfg/code/destruction/rfg_stress.cpp:28` | A piece supported by a dynamic link hands its centre of mass to the other mover's piece in this mover's local coordinates. |

### rfg/effects

| Location | Comment |
|---|---|
| `src/volition/rfg/code/effects/effects_shared.cpp:8` | Visual_effects_parse_effect_shared leaves radial_blur_info_p untouched without a <Radial_blur> element and los_blocker_life_span untouched without a <Blocker>. |
| `src/volition/rfg/code/effects/effects_shared.cpp:10` | Effects_parse_table_shared does not reset the flags or the fade time of an entry it reuses after dropping the previous one. |

### rfg/gameplay

| Location | Comment |
|---|---|
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1518` | 0x00782E40 (:2108). Original quirk kept: the player is not checked for null. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:1647` | 0x00783180 (:2307). Original quirk kept: a squad node with no parent is not a defender. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:2192` | 0x007AD940 (:3041). Original quirk kept: with a dead player the morale still drops, but the activity does not end here. |
| `src/volition/rfg/code/gameplay/activities/morale_effect_manager.cpp:366` | Nothing in rfg.exe turns on the sound propagation mode, which ends here with nothing done. |
| `src/volition/rfg/code/gameplay/gameflow/gfm_tech_level.cpp:241` | The table must hold all five entries. |
| `src/volition/rfg/code/gameplay/managed_layer_loading.cpp:11` | Load passes 0x40 as the size of its 6144-byte file name buffer to cf_drop_ext. |
| `src/volition/rfg/code/gameplay/managed_layer_loading.cpp:12` | Level_init (0x0078DB00) is a 16-byte stub that runs start_layer_unloading. |

### rfg/gamewide

| Location | Comment |
|---|---|
| `src/volition/rfg/code/gamewide/downloadable_content.cpp:23` | Dlc_items_init unloads the items_containers peg (the base game's), not dlc_items_containers. |
| `src/volition/rfg/code/gamewide/downloadable_content.cpp:24` | Dlc_level_init returns without restoring the standard file access or locking Permanent_mempool when a misc or interface packfile does not mount, and without locking it when the audio language is not a language (or past the table); the DLC ... |
| `src/volition/rfg/code/gamewide/downloadable_content.cpp:27` | Dlc_missions_init unlocks Permanent_mempool without having locked it. |
| `src/volition/rfg/code/gamewide/downloadable_content.cpp:28` | Dlc_get_morale_disabled_load_game reads DLC_bundle_info[-1] when no bundle has the current id. |
| `src/volition/rfg/code/gamewide/downloadable_content.cpp:29` | Dlc_path_callback answers past the bundles while Console_dlc_override is not 0 (its default is -1). |
| `src/volition/rfg/code/gamewide/game_clock.cpp:12` | Game_clock_increment_seconds converts the whole days to float as an unsigned value (a negative count of days adds 2^32), and a negative time step adds those days as a huge positive count. |
| `src/volition/rfg/code/gamewide/game_clock.cpp:14` | Free_game_time_trigger moves the following triggers down 4 bytes at a time over (count - index) * 20 bytes. |
| `src/volition/rfg/code/gamewide/game_options.cpp:15` | Read_or_write_ini_file reads and writes tank_stick_controls twice. |
| `src/volition/rfg/code/gamewide/game_options.cpp:16` | The ini file's camera_shake is the inverse of the option's value (read and written inverted). |
| `src/volition/rfg/code/gamewide/game_options.cpp:17` | Parse_display_settings reads performance_mode but only accepts the modes the PC build enumerates: one, GOPM_HIGH_PERFORMANCE, so the mode is always that. |
| `src/volition/rfg/code/gamewide/game_options.cpp:19` | The control configuration of setting 5 is neither read, written nor copied. |
| `src/volition/rfg/code/gamewide/game_options.cpp:20` | Game_options_load_from_cache enables the satellite gyro from the options, then disables it again. |
| `src/volition/rfg/code/gamewide/game_options.cpp:21` | Game_options_are_changed compares both effect volumes (4 and 5) with volume_sfx_interface and does not compare the voice volume 3, aim assist or camera shake.  Split TU (SFX.md 2.6, SX3): compiled twice. The rfg library builds the part the ... |
| `src/volition/rfg/code/gamewide/game_render.cpp:951` | The bloom exposure blend writes Hdr_state back, so with a factor other than 1 it converges on the override exposure over the frames instead of being applied once. |
| `src/volition/rfg/code/gamewide/game_render.cpp:953` | Without a zone at the camera position the render offset is left as it was. |
| `src/volition/rfg/code/gamewide/game_save_generic.cpp:8` | And quirks kept: - game_save_write_thread with a null save_info (the profile import) stores nothing but the profile; a real save_info whose buffer is too large is dropped silently. - game_save_load_thread does not check that the slot ... |
| `src/volition/rfg/code/gamewide/game_stream2_register.cpp:8` | A failed registration of the UI container, the Peg, wavebank or foley bank primitive, or the Landmark LOD primitive logs and returns early; the soundbank failure logs the wavebank message. The other failures only log. |
| `src/volition/rfg/code/gamewide/game_stream2_register.cpp:10` | The Pegs registration reuses the record of the Chunks registration without clearing it (every field is overwritten). |
| `src/volition/rfg/code/gamewide/game_stream2_register.cpp:12` | "Material effect" logs the Glass error message. |
| `src/volition/rfg/code/gamewide/gameseq.cpp:9` | Gameseq_get_previous_state and the pop request read State_stack[Stack_top_index - 1] (Ghidra shows it as an offset from Gamemusic_curr_intensity_factors, the global before State_stack). |
| `src/volition/rfg/code/gamewide/gameseq.cpp:11` | Gameseq_is_state_in_stack scans all 16 slots, including those above the top (stale states of popped entries). |
| `src/volition/rfg/code/gamewide/gameseq.cpp:12` | Gameseq_has_current_state_self_stream_flag with an empty stack reads bit 0 of the byte after State_info[GS_NONE]'s flags, i.e. out of the table (TOD_global_info in Ghidra); here it returns false. |
| `src/volition/rfg/code/gamewide/gamestate_gameplay.cpp:19` | Gamestate_gp_simulate_frame's death handling reads the first player for the listener height (cur_stance) but the local player for the position. |
| `src/volition/rfg/code/gamewide/gamestate_gameplay.cpp:21` | Gamestate_gp_level_init_post calls weather_level_shutdown (not a level init). |
| `src/volition/rfg/code/gamewide/gamestate_gameplay.cpp:22` | The level init timers (timer_get before and after each system, Load_timer_indent +-2) only feed debug output; the indent is kept, the times are not printed. |
| `src/volition/rfg/code/gamewide/preload.cpp:161` | 11 characters into the 10-character extension (it spills into the padding). |
| `src/volition/rfg/code/gamewide/stats.cpp:24` | Stats_serialize_in_save_game_stats reads the bool stats but never sets them, and calls the (null) set function of the percent complete, missions completed and Wrecking Crew stats if they are stored in the save game. |
| `src/volition/rfg/code/gamewide/stats.cpp:26` | Stats_serialize_in_profile_stats updates a stat without a set function with a null delta. |
| `src/volition/rfg/code/gamewide/stats.cpp:27` | Stats_get_achievement_progress reads Achievements[-1] for an unknown id (its null check never fails). |
| `src/volition/rfg/code/gamewide/stats.cpp:28` | Stats_get_req_string_value prints a bool requirement as a critical failure. |

### rfg/geom

| Location | Comment |
|---|---|
| `src/volition/rfg/code/geom/anim.cpp:235` | The rig's start offset is passed as its size. |
| `src/volition/rfg/code/geom/cloth_simulation_shared_spu.cpp:13` | The wind strength moves toward wind_strength_offset in the direction given by comparing wind_heading_offset (not the strength offset) with the current strength. |
| `src/volition/rfg/code/geom/cloth_simulation_shared_spu.cpp:15` | The per-node velocities live in a stack array of 150 entries; nothing checks the node count against it. |
| `src/volition/rfg/code/geom/cloth_simulation_shared_spu.cpp:16` | A non-capsule collider (is_capsule 0) only collides with nodes whose closest point lies inside its segment. |

### rfg/havok

| Location | Comment |
|---|---|
| `src/volition/rfg/code/havok/api/hkcharactercontroller.cpp:738` | Hp is dereferenced here (and below) without a check; the listener's human exists whenever a proxy integrates. |
| `src/volition/rfg/code/havok/api/hkcharactercontroller.cpp:757` | In spirit: the binary reads the wall's sub_type even when no object was found (a null read); here a missing object counts as not a vehicle. |
| `src/volition/rfg/code/havok/havok.cpp:19` | Get_highest_point moves the ray into body space with the body rotation itself (setRotatedDir) and back with its transpose, which is only right for the unrotated terrain bodies it is used on. |
| `src/volition/rfg/code/havok/havok.cpp:21` | Havok_set_status (and its inlined copies in havok_update_status) folds the leftover step time into one step (fmod) even when the status does not change. |
| `src/volition/rfg/code/havok/havok_bp.cpp:16` | Island_alloc puts new islands in the LEAVING list. |
| `src/volition/rfg/code/havok/havok_bp.cpp:17` | The thread checks (havok_bpo_validate_updates, inlined in every entry point) only read the thread id. |
| `src/volition/rfg/code/havok/havok_bp.cpp:18` | Process_new_pairs and process_deleted_pairs ignore their pair list id. |
| `src/volition/rfg/code/havok/havok_cm_manager.cpp:14` | Havok_cm_manager_create_sphere_shape keeps its own reference on the sphere when it wraps it in an hkpConvexTranslateShape (the translate shape adds a second one), so an offset sphere is never freed. |
| `src/volition/rfg/code/havok/havok_cm_manager.cpp:16` | Havok_cm_manager_add returns 0 (the default model) instead of an invalid index when the table is full or not initialized. |
| `src/volition/rfg/code/havok/havok_cm_manager.cpp:18` | Havok_cm_manager_load_all ignores max_cm_indices. |
| `src/volition/rfg/code/havok/havok_collision_events.cpp:18` | Havok_collision_events_add_contact_event drops a queued event of the same pair instead of adding another (a contact that starts and ends within one step raises no event); the event type is not compared. |
| `src/volition/rfg/code/havok/havok_collision_events.cpp:20` | Havok_collision_events_end_sim reads the unary callback of type -1 before checking the type (the check is done first here). |
| `src/volition/rfg/code/havok/havok_memory.cpp:13` | Havok_mem_display_stats and havok_memory_listener::allocationFailure format their report into a local buffer that is never printed (the print was compiled out of the retail build). |
| `src/volition/rfg/code/havok/havok_memory.cpp:15` | Watch_dog_remove_objects_from_island and havok_memory_watch_dog_free_memory destroy World's null object when an entity has no object property (world::destroy_object ignores it); either way the entity counts as removed. |
| `src/volition/rfg/code/havok/havok_memory.cpp:17` | FreeHeapMemoryTillRequestedAmountIsAvailable runs its removal loop per memory statistics entry, with a remove level from that entry's total available memory, and counts entries in a float. |
| `src/volition/rfg/code/havok/havok_memory.cpp:19` | Havok_memory_init rounds Havok_mem_largest_runtime_block up to the largest of its steps first, so only 0xFD800 is ever chosen and larger blocks stay as they are. |
| `src/volition/rfg/code/havok/havok_memory.cpp:21` | Havok_memory_shutdown destroys the listener with its deleting destructor, which does not free (the listener lives in Havok_memory_listener_local). |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:27` | Havok_sync_ragdoll_to_anim computes the pelvis matrix from the bone end before it is overwritten by the generic bone start one (no effect), and fetches the animation's bone velocities without using them (not ported). |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:29` | Havok_ragdoll_emergency_cleanup and havok_ragdoll_bind_to_human write to the human of a bound slot without checking that it still exists. |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:31` | Havok_ragdoll_get_pos with RAGDOLL_ROOT reads bodies[-1]. |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:32` | Havok_collect_ragdolls_fast never checks max_collected. |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:1669` | The saved ray start is applied as a point impulse at the saved end (see the file comment). |
| `src/volition/rfg/code/havok/havok_raycast.cpp:12` | Havok_raycast_group builds its collectors with a placement new[] whose array cookie is not part of the Common_mempool allocation, so the last collector reaches 16 bytes past it. The collectors are never destroyed (the pool is restored to ... |
| `src/volition/rfg/code/havok/havok_raycast.cpp:15` | Havok_closest_point_per_object_collector compares the objects of two bodies, and two bodies without an object (property 0) count as the same object. |
| `src/volition/rfg/code/havok/havok_raycast.cpp:17` | Havok_get_objects_closest_point reports the distance divided by the query radius as the hit fraction. |

### rfg/hud

| Location | Comment |
|---|---|
| `src/volition/rfg/code/hud/fog_of_war.cpp:199` | The block is not cleared, so name_checksum, constant_name_checksum, the descriptors' upper bits and mat_flags (only ORed below) keep whatever the heap left there. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:2181` | Kept: get_best_time's bool is not read, and when it does not write the time this is the slot get_activity_rewards just wrote, the salvage. |
| `src/volition/rfg/code/hud/hud.cpp:212` | The last offset is read even when the loop ran off the list (offset null); only the name check below keeps it from being used, and that compares the last name again. |
| `src/volition/rfg/code/hud/hud.cpp:265` | The first part is read without a check. |
| `src/volition/rfg/code/hud/hud.cpp:305` | "HUD" is read into the ally health position, overwritten by the next line. |
| `src/volition/rfg/code/hud/hud_icon.cpp:199` | The text's alpha comes from the icon's alpha, already faded, faded again. |
| `src/volition/rfg/code/hud/minimap.cpp:349` | Bmin - bmin, as above. |
| `src/volition/rfg/code/hud/minimap.cpp:980` | The vehicle's position is read and overwritten by the player's. |
| `src/volition/rfg/code/hud/minimap.cpp:1373` | The bitmap's uvs turned in place, inlined in both render_bitmap_oriented. Original quirk kept: bui is the caller's, so a shared cached_bmp_info stays turned. |
| `src/volition/rfg/code/hud/minimap.cpp:1537` | On a failed allocation the strip is ended here, the caller's begin is its own. |
| `src/volition/rfg/code/hud/minimap.cpp:1883` | Np is not checked here. |
| `src/volition/rfg/code/hud/minimap.cpp:1892` | Np is not checked here. |
| `src/volition/rfg/code/hud/minimap.cpp:2625` | EDF movers use their own right vector, the others one made from the forward vector. |
| `src/volition/rfg/code/hud/minimap.cpp:3423` | The decal's bitmap (a bitmap id) is read as a map element here. |
| `src/volition/rfg/code/hud/minimap.cpp:3751` | The stored element, not the one the height changed, is tested. |
| `src/volition/rfg/code/hud/minimap.cpp:3967` | The closest icons don't scale with the zoom, the pulse's part does. |
| `src/volition/rfg/code/hud/minimap.cpp:5290` | The count is read once, a removal skips the next icon this frame. |
| `src/volition/rfg/code/hud/minimap.cpp:5884` | District == size and index == num_indices pass. |

### rfg/interface

| Location | Comment |
|---|---|
| `src/volition/rfg/code/interface/ingame/interface.cpp:59` | INT_A, INT_B, INT_X and INT_Y have lengths past their names (5, 10, 12, 10), so more of the text than the token can match and go. |
| `src/volition/rfg/code/interface/ingame/overlay.cpp:92` | Reads elt[-1] with an empty stack. |
| `src/volition/rfg/code/interface/ingame/ovl_inventory.cpp:435` | The previous slot is selected even when the default weapon was equipped. |
| `src/volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.cpp:1129` | No break, the playlist text is cleared by the default case. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp:469` | A name without a localized string is a null pointer here, for the levels and the modes. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp:852` | The document stays loaded on exit. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.cpp:9` | (commented): the mouse hint flag and the events flag start uninitialized in rfg.exe; an option once disabled is never enabled again. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.cpp:524` | Rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.cpp:610` | These are not set true again if the client later offers them. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp:7` | (commented): the document stays loaded on exit; the events flag starts uninitialized in rfg.exe. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp:453` | Rfg.exe never initializes the flag (its slot is ECX, pushed as stack space); false here. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp:544` | The document is not unloaded (Mp_live_find_servers_info.doc_handle stays). |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp:7` | (commented): compare_search_region is not a consistent ordering; the document stays loaded on exit; sorted_player_list is never filled, so the player options box is unreachable. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.cpp:6` | (commented): party_lobby_get_prev_type goes forward; the document stays loaded on exit. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.cpp:1053` | The document stays loaded on exit. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.cpp:6` | (commented): the document stays loaded on exit; the events flag starts uninitialized in rfg.exe. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.cpp:387` | Rfg.exe never initializes the flag (its slot is ECX, pushed as stack space); false here. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.cpp:470` | The document is not unloaded (Party_options_menu_info.doc_handle stays). |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp:6` | (commented): compare results are not a consistent ordering for a player on equal rank; a board change ignores the filter passed in; a failed data item create reuses the previous handle. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp:202` | The start of my rank's page. Original quirk: a rank right after a page boundary gets one less (rank 17 gives 16). |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.cpp:361` | Rfg.exe selects the pages by name too, but its swap copies the page one past the last onto itself, so the pages stay in the table's order (the copy past the end is not reproduced). |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.cpp:449` | Rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.cpp:474` | Rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp:6` | (commented): the video's volume shrinks with each play; the document stays loaded on exit; an empty list divides by zero. |
| `src/volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.cpp:897` | The document stays loaded on exit. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:623` | The group and the text are not checked. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:1306` | The text is not checked. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:1372` | The text is used below without a check. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:2033` | No check against the 192 bytes of name. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:2574` | The text is the format string. |
| `src/volition/rfg/code/interface/ui_document_hud.cpp:2738` | The text is the format string. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:320` | The frame the group goes away the rest is skipped; the next frame creates it. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:746` | The level is not checked here (it is at the end of a round below). |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:962` | With more players than one the second entry stays. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:1317` | The item is not checked. |
| `src/volition/rfg/code/interface/ui_document_hud_multiplayer.cpp:1869` | The localized text is not checked for null. |
| `src/volition/rfg/code/interface/ui_document_hud_satellite.cpp:6` | (commented): a mission's description and caption are used as format strings, and the header sends an uninitialized buffer when the mission has no description; the hint item's show flag is set twice. |
| `src/volition/rfg/code/interface/ui_document_hud_satellite.cpp:275` | Not initialized, and sent as it is without a description. |
| `src/volition/rfg/code/interface/ui_document_hud_tool_tips.cpp:227` | The player is not checked here. |
| `src/volition/rfg/code/interface/ui_document_hud_weapons.cpp:406` | Compared before this frame's selection is determined, so a change shows one frame later. |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:769` | The project's include pegs (without the platform button pegs), then the platform button pegs. Original bug kept: the second list is skipped only when the peg after the project's last ("interface-pc") is that name. |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:888` | The removal shifts the next entry into i, which the loop then skips, so those names leak. The bound is the old count (0x008C7277), so the walk goes past the new end and the count ends at the mark. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:230` | The map's menu image is released when it is loaded or loading. Original bug kept: for the old map (old_map_index) the loading test reads the current map's image (map_index) instead. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:892` | Up/down: the selection wraps; a disabled row is stepped over (Original bug kept: stepping up from the weapon row in a challenge, reachable only with the mouse, leaves a negative selection). |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:1029` | The challenge: random weapon and backpack, on the first map from here that has one. Original bug kept: with no challenge map at all this loops forever. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:1085` | Locked maps (and, in a challenge, maps without one) are stepped over. Original bug kept: with no left/right a locked current map loops forever. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:1106` | -1 is random. Original bug kept: with no left/right an unavailable weapon loops forever. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:1117` | -1 is random. Original bug kept: with no left/right an unavailable backpack loops forever. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:1218` | The index equal to the character count passes as valid. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:8` | (commented): compare results are not a consistent ordering for a player on equal rank; a board change ignores the filter passed in; the board cycle steps map indices modulo the number of challenge modes and loops forever when no map has a ... |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:200` | The start of my rank's page. Original quirk: a rank right after a page boundary gets one less (rank 17 gives 16). |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:250` | A board change ignores the filter (the callers pass none): the request's filter stays. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:289` | An equal rank compares as greater both ways, not a consistent ordering for qsort. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:442` | A failed create keeps the previous item's handle (0 for the first). |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:988` | The next map with a challenge. Original bugs: map indices are stepped modulo the number of challenge modes, and the loop never ends when no map has a challenge. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:1133` | The board is the map index + 4 here, the challenge's leaderboard id + 4 once cycled (change_request_wcscb), and map 0 is taken without checking it has a challenge. |
| `src/volition/rfg/code/interface/ui_document_shared.cpp:90` | The file stays open when it is empty or cannot be read. |

### rfg/level

| Location | Comment |
|---|---|
| `src/volition/rfg/code/level/rfg_chunk_shared.cpp:11` | Chunk_base_load takes the pool slot and advances C_chunk_base_uid before it checks whether the chunk is already loaded, so a duplicate load burns a uid. |
| `src/volition/rfg/code/level/rfg_chunk_shared.cpp:13` | Chunk_load_collision_models ignores the result of havok_cm_manager_load_all; on failure the handles of the previous chunk left in hkShapeHandleScratchMem are used. |
| `src/volition/rfg/code/level/rfg_prop_block_shared.cpp:10` | Get_buf_copy / get_buf_copy_optional copy 0 bytes from the prop_name when the property is missing. |
| `src/volition/rfg/code/level/rfg_prop_block_shared.cpp:11` | Set_flags_as_str ORs a stale value for a key whose lookup fails (cannot happen: the keys come from the table). |
| `src/volition/rfg/code/level/rfg_prop_block_shared.cpp:12` | Get_str_as_enum only formats its "Could not find object parameter" message into the thread's scratch buffer. |
| `src/volition/rfg/code/level/rfg_stream_layout_shared.cpp:18` | Get_cell_list does not clamp to the grid: positions near the edge give indices outside the cell array, which stream_grid::do_frame reads without checking. |
| `src/volition/rfg/code/level/rfg_stream_layout_shared.cpp:20` | Create_cell_mask_region stores uninitialised region handles for the broadphases that do not exist (zero here). |
| `src/volition/rfg/code/level/rfg_stream_layout_shared.cpp:21` | Sl_slurp_zone_and_terrain creates the stitch containers from the first stitch record only, once per record. |
| `src/volition/rfg/code/level/rfg_stream_layout_shared.cpp:22` | Stream_layout_level_init returns false without disabling standard access again when a packfile is missing. |
| `src/volition/rfg/code/level/rfg_terrainx.cpp:20` | Terrain_load_terrain keeps the rfg_terrain it took from Terrains when a later step fails. |
| `src/volition/rfg/code/level/rfg_terrainx.cpp:21` | An already registered file name keeps the old entry in Terrain_filename_hash (the new terrain is still loaded). |
| `src/volition/rfg/code/level/rfg_terrainx.cpp:22` | LTCG: terrain_instantiate_stitch_physics takes the chunk base in EDI and the rotation in ESI. |
| `src/volition/rfg/code/level/rfg_zonex_shared.cpp:12` | Get_territory_files lists the persistent zone ("<territory>_persistent_15_00") first, but for a territory the name filter then drops it again (its territory name parses as "<territory>_persistent"). |
| `src/volition/rfg/code/level/world.cpp:25` | Level_shutdown resets the stream layout itself (the inlined stream_layout_level_shutdown) and so does shutdown_territory_streaming_resources. |
| `src/volition/rfg/code/level/world_serialize.cpp:35` | Deserialize_object writes the zone_object sibling, child and num fields uninitialized. |
| `src/volition/rfg/code/level/world_serialize.cpp:36` | Create_object_internal uses an uninitialized position when neither OBJ_PROP_JUST_POS nor OBJ_PROP_COMBINED exists. |
| `src/volition/rfg/code/level/world_serialize.cpp:37` | Serialize_zone keeps the chunk name of the previous rfg_mover when one has no OM_PROP_CHUNK_NAME. |
| `src/volition/rfg/code/level/world_serialize.cpp:1471` | A save too large for a slot is dropped without a message (and Common_mempool keeps the autosave list until the next restore). The year is the two digits os_get_date gives, masked to a byte like the other fields. |

### rfg/multi

| Location | Comment |
|---|---|
| `src/volition/rfg/code/multi/multi_core/generic/multi_voice_generic.cpp:209` | 0x009715B0 (:248). Original bug kept: the result of getVoiceChatLocalPlayerInfo is not checked (as above). |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_io.cpp:552` | Packets are queued instead. Original bug kept: a type of 0x80 passes the range check (Multi_packets has 128 entries). |
| `src/volition/rfg/code/multi/multi_game/multi_game_overrides.cpp:85` | 0x00981650 (:208). LTCG: pool in EAX. Original bug kept: an entry naming an unknown backpack is never left (the next entry is only looked up after a known one), so the loop does not end; the shipped table names known ones. |
| `src/volition/rfg/code/multi/multi_game/multi_game_settings.cpp:1157` | The sets of the failed pass stay allocated, so this ends only when the pool is used up (the shipped table names each base set before its users). |
| `src/volition/rfg/code/multi/multi_game/multi_game_settings.cpp:1825` | 0x00984B30 (:3290). Original bug kept: an index out of range reads the crc of a null level. |
| `src/volition/rfg/code/multi/multi_game/multi_game_xp.cpp:432` | A part of level 1 still takes a slot (left as it was in the array). |
| `src/volition/rfg/code/multi/multi_game/multi_game_xp.cpp:767` | (rfg.exe also compares the player index with -1, which a 16-bit index never is.) Original bug kept: index 16 (the spectator slot) is the first team timer. |
| `src/volition/rfg/code/multi/multi_game/multi_game_xp.cpp:899` | 0x00967950 (:1191): the weapon unlock before this one (wrapping). Original bug kept: from no weapon (-1) the index goes negative after the array's start and reads before it. |

### rfg/objects

| Location | Comment |
|---|---|
| `src/volition/rfg/code/objects/gameplay_properties.cpp:9` | Gpm_parse_table_file does not advance to the next entry when one fails to parse, so a bad entry loops forever. |
| `src/volition/rfg/code/objects/gameplay_properties.cpp:10` | Gpm_parse_gameplay_property copies the name into the pooled string again after add_unique. |
| `src/volition/rfg/code/objects/gameplay_properties.cpp:11` | Gpm_dlc_shutdown re-reads the base table into the existing array. |
| `src/volition/rfg/code/objects/humans/human_collide.cpp:10` | Human_collide_calculate_damage_with_timer returns 0 for every hit within 500 ms of the previous one (the stored damage is raised to the new one before the difference is taken), and each such hit restarts the timer. |
| `src/volition/rfg/code/objects/humans/human_collide.cpp:12` | Human_collide_vehicle dereferences the human without checking that the handle resolved to one. |
| `src/volition/rfg/code/objects/humans/human_collide.cpp:13` | Player_collide_npc dereferences the npc after its null check. |
| `src/volition/rfg/code/objects/humans/human_ragdoll.cpp:12` | Human_find_place_to_stand_check scores the remaining candidates around the last checked one by its error; for an error without a case (too low) it reuses the previous center, and for a vehicle it uses the vehicle's half diagonal from the ... |
| `src/volition/rfg/code/objects/humans/human_ragdoll.cpp:15` | Human_ragdoll_get_up_check::get_errors reads the terrain height even when there is no terrain there, and sets the vehicle error of a blocking object found by the capsule check from the object of the last ground hit. |
| `src/volition/rfg/code/objects/humans/human_ragdoll.cpp:17` | Human_ragdoll_get_up: a place found within 2 m is only used as the position for update_pos_and_orient. |
| `src/volition/rfg/code/objects/humans/human_spinebend.cpp:1015` | Pp is used unchecked below (always the player here, as is_player is set only for one). |
| `src/volition/rfg/code/objects/level_object_properties.cpp:8` | Lopm_free_instance frees the script point effect instances but leaves the pointers in the freed instance. |
| `src/volition/rfg/code/objects/level_object_properties.cpp:9` | Level_object_inst_properties::apply_damage ignores DBT_WORLD and DBT_RADIATION (and the dislodge hitpoints for them). |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:10` | Lopm_parse_level_object_property loops forever when a DeathExplosionList entry names an unknown explosion (the element is not advanced on that path). |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:12` | A missing Prefix_Effects/Max_Duration resets the min duration instead of the max one. |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:13` | M_explosion_prefix_min/max_duration, m_prefix_hitpoints and m_release_looping_ms are only written when the entry has Prefix_Effects / Release_Effect; otherwise they keep the caller's (stack) garbage. |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:15` | The "Multiple ... impact override effects" messages are formatted but never printed. |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:16` | Lopm_parse_table_file returns without closing the table when an entry fails to parse. |
| `src/volition/rfg/code/objects/level_object_properties_shared.cpp:17` | The DLC table is allocated from the pool again by every DLC init; lopm_dlc_shutdown only forgets it. |
| `src/volition/rfg/code/objects/lod_properties.cpp:8` | Lpm_parse_table_file does not advance to the next entry when one fails to parse, so a bad entry loops forever. |
| `src/volition/rfg/code/objects/lod_properties.cpp:9` | Lpm_system_init formats a memory report that is never printed. |
| `src/volition/rfg/code/objects/movers/general_mover.cpp:16` | Create_deferred_ri (also inlined in general_mover_pre_stream_process_all) reads the instance of the resource data without checking that it is loaded. |
| `src/volition/rfg/code/objects/movers/general_mover.cpp:18` | General_mover::apply_damage with a pending prefix effect (gm_flags 0x10) only restarts the death timer: the destroyed event, activity death and control drop are skipped for that hit. |
| `src/volition/rfg/code/objects/movers/general_mover.cpp:20` | Get_tag_local_coords returns false without touching the outputs when the chunk is missing. |
| `src/volition/rfg/code/objects/movers/object_mover.cpp:16` | Maybe_start_nano_dissolve checks gm for nullptr only for the first tests and dereferences it afterwards. |
| `src/volition/rfg/code/objects/movers/object_mover.cpp:17` | Object_mover_mark_for_fade sets the timer of a nullptr mover. |
| `src/volition/rfg/code/objects/movers/object_mover.cpp:18` | Object_movers_record_starting_points sorts by descending sum of distances with a comparator that never returns 1. |
| `src/volition/rfg/code/objects/movers/object_mover.cpp:19` | Script_point_finish_death calls script_point_destroy even when the effect was not stopped. |
| `src/volition/rfg/code/objects/object.cpp:17` | Serialize(load) reads the position from OBJ_PROP_COMBINED into an uninitialized local when neither it nor OBJ_PROP_JUST_POS is present. |
| `src/volition/rfg/code/objects/object.cpp:19` | Set_child / remove_child free attach_info into Attach_info_pool (file static in human.cpp). |
| `src/volition/rfg/code/objects/object_convoy.cpp:933` | (asm 0x00A61B1C): a squad that was in the formation returns right after leaving it, so neither the formation update nor the activity's notice runs for it. |
| `src/volition/rfg/code/objects/object_convoy.cpp:1431` | An unknown type only formats a message; the definition stays with CONVOY_TYPE_UNDEFINED. |
| `src/volition/rfg/code/objects/object_debris.cpp:11` | Object_debris_free_all and the console command walk the type list while destroying from it. |
| `src/volition/rfg/code/objects/object_item.cpp:2081` | The offset is computed into pos itself, so pos becomes twice the rotated offset (the item's position is lost); the orientation is the tag's inverse rotation times the object's. |
| `src/volition/rfg/code/objects/object_path_road.cpp:183` | Reads a second point even for a road with one. |
| `src/volition/rfg/code/objects/object_path_road.cpp:806` | 0x00A69050 (:1349): Original quirk kept: use_special_roads asks for special roads only. |
| `src/volition/rfg/code/objects/salvage.cpp:57` | An unknown material name indexes Salvage_material_data with -1, a material with no Item_3d is read again for ever because the next material is only found inside that branch, and Salvage_items has no bounds check. |

### rfg/pathfinding

| Location | Comment |
|---|---|
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:227` | The turning direction bit is only ever set here, never cleared. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:958` | A bridge node with no linked bridge node goes on to read adjacent_cells[arrival_edge], past the array when the edge is 3 or more. Nothing here guards it in rfg.exe; such data would break it. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:1074` | With one node left the off-mesh nodes are neither put back nor freed. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:1404` | An unscripted vehicle path takes the pedestrian limit. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:1808` | This path leaves result at PR_UNKNOWN. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:3311` | With no links at all best_link is null here; a path with a next node always has one. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:3392` | A second node without a first one is never handed back. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:3492` | From the asm's arguments: stuck travels in ignore_set's slot, the EDF combat flag in use_doors' and the scripted flag in encourage_edf_spread's; the path to avoid is none. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:4083` | Magic number: type 19, the navpoints of holes. Original quirk kept: only navpoints are ever collected. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:4540` | The handle is not checked for a live object. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:4861` | The first hit's position, whichever hit matched. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:5927` | A request without vehicle_pfg calculates nothing; rfg.exe then reads an unset result. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:6297` | Only the low byte of the index is looked at. |
| `src/volition/rfg/code/pathfinding/bb_pathfinding.cpp:6307` | No check for a free node. |
| `src/volition/rfg/code/pathfinding/nav_cell.cpp:642` | A zone without a human graph is not searched for vehicle cells either. |
| `src/volition/rfg/code/pathfinding/nav_cell.cpp:1131` | A light cell blocks only when disabled cells are allowed. |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:678` | 0, not NCD_NONE; the hull goes back on the free list and is reset when reallocated. |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:1228` | The kept intersection point takes no jitter. |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:1350` | Renames the block references of hull old_ref to new_ref. Original bug kept where a caller skips the guard nodes. |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:1488` | This start point takes its jitter from hull0, not from other. |
| `src/volition/rfg/code/pathfinding/nav_cell_detour.cpp:2194` | VCT_DOOR is a hash global (vehicle_component.cpp), compared by value. |
| `src/volition/rfg/code/pathfinding/nav_heap.cpp:241` | No check against the 512 entries of heap. |

### rfg/scripting

| Location | Comment |
|---|---|
| `src/volition/rfg/code/scripting/action_script/action_script.cpp:186` | The comparison with the best one so far uses <= mark, not the best's own mark. |
| `src/volition/rfg/code/scripting/action_script/global_event_manager.cpp:284` | The early exit on a freed-and-reused entry leaves without restoring the mark. |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4588` | 0x00B28D30: original quirk kept: the return value is the child's handle even when no child spawned. |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:5333` | 0x00B2AC90: original quirk kept: the first test reads the subtype without the type, which holds for npcs only. |

### rfg/vehicle

| Location | Comment |
|---|---|
| `src/volition/rfg/code/vehicle/automobile.cpp:1595` | Not checked for being loaded. |
| `src/volition/rfg/code/vehicle/automobile.cpp:1627` | A handle that is no automobile reads through a null pointer. |
| `src/volition/rfg/code/vehicle/automobile.cpp:1686` | Not checked for being loaded. |
| `src/volition/rfg/code/vehicle/automobile.cpp:1907` | A handle that is no automobile reads through a null pointer. |
| `src/volition/rfg/code/vehicle/automobile.cpp:2013` | A component that is no wheel indexes with -1. |
| `src/volition/rfg/code/vehicle/automobile.cpp:2091` | A handle that is no automobile reads through a null pointer. |
| `src/volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.cpp:66` | A null owner_vehicle_instance passes the first test and is then dereferenced. |
| `src/volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.cpp:118` | A null owner_vehicle_instance passes the first test and is then dereferenced. |
| `src/volition/rfg/code/vehicle/havok/hk_custom_vehicle_raycast_wheel_collide.cpp:335` | Every key is written to index 0, so the last key of the hierarchy is what stays. |
| `src/volition/rfg/code/vehicle/havok/hk_custom_vehicle_transmission.cpp:139` | A vehicle that is not an automobile passes a null vehicle here. |
| `src/volition/rfg/code/vehicle/vehicle_audio.cpp:296` | A null instance reads the forward limit through null. |
| `src/volition/rfg/code/vehicle/vehicle_audio.cpp:517` | A reversing vehicle fades over 650 ms |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:156` | A vehicle from a handle; rfg.exe's checks inlined everywhere. Original bug kept by the callers that do not test it. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:391` | The lights of one tag name; continues the vehicle's list. Original quirk: 20 lights at most. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:518` | The mesh's turret components, and whether a mount below uses each. Original bug kept: 6 at most, no bound. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:1096` | Breadth first from the root components. Original bug kept: 66 entries, no bound. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:1384` | "<=" lets one past the last child through. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:1784` | The fire effect's handle is cleared through the smoke's slot. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:1824` | Without loaded data these read through null. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:3403` | The weight is read at the whole tag index, which here equals the tag. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:3829` | Only the first passenger's seat is looked at. |
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:5048` | Not checked for loaded data; automobile's stream code keeps it in step with rdd. |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:507` | Not checked for null. Under 10 m/s it holds on with a hit point. |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:1261` | The first test is against 0.99 (an axis nearly parallel to all three leaves perp unset; a unit axis cannot be). |
| `src/volition/rfg/code/vehicle/vehicle_component.cpp:1558` | The chassis space point is added to the position without the vehicle's rotation. |
| `src/volition/rfg/code/vehicle/vehicle_effects.cpp:515` | Not checked for null |
| `src/volition/rfg/code/vehicle/vehicle_flyer.cpp:960` | Turning one way only. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:515` | A walker on its mount with nothing under it has nowhere to put the human down. Original bug kept: the cast is checked and a NULL walker would be read. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:665` | Rfg.exe's cast can give NULL when the object type is wrong. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:1126` | Always the first hit's centre, whichever hit passed the test. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:1664` | A human with a seat only looks at the seats before it. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:3500` | A door_idx of -1 reads the slot before the array. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:3571` | A -1 node indexes before the waypoints here. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:4398` | Rfg.exe's cast can give NULL when the object type is wrong. |
| `src/volition/rfg/code/vehicle/vehicle_interaction.cpp:5106` | A handle that is no longer a human writes through NULL. |
| `src/volition/rfg/code/vehicle/vehicle_interaction_info.cpp:408` | Without an object of type human this reads through null. |
| `src/volition/rfg/code/vehicle/vehicle_render.cpp:24` | Asks for the time of day for each occupied vehicle in view, and does nothing with it. |
| `src/volition/rfg/code/vehicle/vehicle_system.cpp:492` | A bad handle calls through null. |
| `src/volition/rfg/code/vehicle/vehicle_system.cpp:698` | A display name no longer in the table gives entry -1. |
| `src/volition/rfg/code/vehicle/vehicle_table_parser.cpp:1672` | More than 16 groups fails without closing the table. |
| `src/volition/rfg/code/vehicle/vehicle_table_parser_shared.cpp:93` | The name is copied before the check, duplicates leave their copy in the pool. |
| `src/volition/rfg/code/vehicle/walker.cpp:1122` | Mounted: the driver goes along as a passenger of the vehicle. Original bug kept: no driver is not checked for. |
| `src/volition/rfg/code/vehicle/walker.cpp:2070` | Nobody in it is not checked for. |

### rfg/vehicle_ai

| Location | Comment |
|---|---|
| `src/volition/rfg/code/vehicle_ai/air_traffic_control.cpp:96` | Higher than every reservation goes to the bottom of the list, not the top. |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:530` | No_raycast is what asks for the raycast here, the opposite of the other branch. |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:1200` | Uses the box height, which stays 0 when the box test hits nothing. |
| `src/volition/rfg/code/vehicle_ai/vehicle_ai.cpp:1741` | Is written and read back. Original bug kept in spirit: the slot is only written for an index below 5 here. |
| `src/volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp:117` | With two points in the list the insert index can only be 1, so this is its segment. |
| `src/volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp:143` | Jumping and the rest keep the bytes of the point moved away. |
| `src/volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.cpp:559` | Always the second point, which is the only one this can be with two points. |

### rfg/weapons

| Location | Comment |
|---|---|
| `src/volition/rfg/code/weapons/weapons.cpp:2304` | The loop runs over the main ray's hit count while reading the muzzle ray's hits (checked in the asm), and the result is whether the main ray hit. |

### stream2

| Location | Comment |
|---|---|
| `src/volition/stream2/code/stream2.cpp:13` | Stream2_container_finish without a container leaves the build stack as it is. |
| `src/volition/stream2/code/stream2.cpp:14` | Stream2_read_container_data abandons a container whose primitive cannot be added (its build is not finished and the rest of its records are not skipped) and continues only with patch_data. |
| `src/volition/stream2/code/stream2.cpp:16` | Q_sanity_check walks the container list (next) instead of the queues (q_next). |
| `src/volition/stream2/code/stream2.cpp:17` | Stream2_system_init ignores the task system; stream2_container_system_init clears the primitive types but not the container types. |
| `src/volition/stream2/code/stream2_allocator.cpp:4` | Stream2_dynamic_allocator uses cpu_memmgr for both halves (gpu_memmgr is never used); its max_alloc reports the CPU manager's largest block for the GPU side too. alloc fails without touching cpu_alloc when the request is larger than the ... |
| `src/volition/stream2/code/stream2_allocator.cpp:7` | Stream2_mempool_allocator and stream2_stack_allocator place the GPU part right after the CPU part (aligned to gpu_align, the larger alignment for the block) when the pool has no separate GPU pool; free does nothing for the mempool ... |
| `src/volition/stream2/code/stream2_allocator.cpp:10` | Stream2_stack_allocator::alloc(bytes) records the mark in cpu_mark and the data in cpu_data (gpu_mark and gpu_data stay 0). free(data1, data2) on a single pool calls free(data1). |
| `src/volition/stream2/code/stream2_allocator.cpp:12` | Stream2_slot_allocator ignores the GPU sizes of bracket_info (gpu_alloc_cb is only checked for null): each slot is one mempool of cpu_align + gpu_align + cpu_size bytes from the cpu_alloc_cb block, and alloc hands out the whole slot ... |
| `src/volition/stream2/code/stream2_allocator.cpp:16` | Stream2_system_allocator always allocates the GPU part, even when gpu_bytes is 0. |
| `src/volition/stream2/code/stream2_container.cpp:19` | In SCM_PRIM alloc a primitive with a GPU size whose type is not alloc_split gets a staging-block pointer that is then replaced by the address after its CPU part, which its allocation does not cover. |
| `src/volition/stream2/code/stream2_container.cpp:21` | The 6-argument calc_container_requirements does not exclude chunks from the GPU block and treats a primitive named exactly "vehicle" as a normal one; entry flags (entry::flags) are left uninitialised by add_prims (0 here). |
| `src/volition/stream2/code/stream2_container.cpp:23` | Stream_wait skips the container type's load callback when a primitive's callback fails, but still marks the container loaded. |
| `src/volition/stream2/code/stream2_os.cpp:12` | The device constructor pushes every request at the front of the free list. |
| `src/volition/stream2/code/stream2_os.cpp:13` | Read() never puts a request in front of the queue head (the head may be in service): it scans from the tail for the first request of at least the same priority and inserts after it, or after the head. |
| `src/volition/stream2/code/stream2_os.cpp:15` | Stream2_os_read dereferences a null request; when cf_find_file fails the media type is not set (zero here, HDD). A Disc media type selects device index -1 unless Disc_build. |

### vfxlib

| Location | Comment |
|---|---|
| `src/volition/vfxlib/code/vfx_effect.cpp:495` | 0x00CB1BC0 (:396): everything goes back to its pool and the effect to the manager's free list. Original quirk kept: the renderable filter instances are released in place (their list is left as is, the next create reinitializes it). |
| `src/volition/vfxlib/code/vfx_effect.cpp:678` | Death kills them. Original bug kept: a node emitter missing from the effect's list is dereferenced. |
| `src/volition/vfxlib/code/vfx_effect_expressions.cpp:31` | The game variables. Original quirk kept: Registered_variables holds 100 names and the count is not checked, so a 101st variable would index past the array. |

### vlib

| Location | Comment |
|---|---|
| `src/volition/vlib/code/bmpman/bmpman.cpp:17` | Bm_add_peg returns without unlocking Bm_add_lock when the name is already registered (and when it cannot be added to Bm_pegs, the table is full). The lock is recursive, so the thread that loaded the peg twice goes on; any other thread that ... |
| `src/volition/vlib/code/bmpman/bmpman.cpp:21` | Bm_register_peg advances by num_frames entries per registered bitmap; a found bitmap with num_frames 0 would never advance. bm_unregister_peg likewise never advances past a peg entry whose name has no handle. Both loop forever in those ... |
| `src/volition/vlib/code/bmpman/bmpman.cpp:24` | Bm_register_peg adds a new name with bm_add_bitmap(peg_entry*) and then advances by one entry even for an animated bitmap: the following frame entries are then found by name and linked to the first frame's handle. |
| `src/volition/vlib/code/bmpman/bmpman.cpp:26` | Bm_load_peg(filename, pool, peg, ...) writes the 'g' of the GPU extension into a string literal ("") when the name has no extension. |
| `src/volition/vlib/code/bmpman/bmpman.cpp:28` | The frame of an animated bitmap (bm_get_entry with auto_animate) follows Frametime_total_milliseconds. |
| `src/volition/vlib/code/cfile/cfile.cpp:6` | Only memory files can be written; writes to any other file fail with CF_ERROR_WRITE. |
| `src/volition/vlib/code/cfile/cfile.cpp:7` | Cf_open_lock only records the flag (cf_is_locked); no open path checks it in this build. |
| `src/volition/vlib/code/cfile/cfile.cpp:8` | Cf_find_file_io_vdir and the CF_SEARCH_VDIR branch of cf_find_file print "This should not be called...". |
| `src/volition/vlib/code/cfile/cfile.cpp:9` | Cf_read_float/cf_write_float byte-swap the value rounded to an integer, not its bits. |
| `src/volition/vlib/code/cfile/cfile.cpp:10` | Cf_write_string writes the 16-bit length without byte-swapping it. |
| `src/volition/vlib/code/cfile/cfile.cpp:11` | Cf_find_ext returns the number of names found (the original returns a stale register); unlike cf_find_ext_path and cf_find_pattern it does not take Cfile_search_cs. |
| `src/volition/vlib/code/cfile/cfile.cpp:13` | Cf_open_internal leaks the cfile when a search system it does not know reports a hit. |
| `src/volition/vlib/code/cfile/keen_cfile.cpp:7` | Files are read sequentially; the position is only set before a read when the file was opened async (m_flags 0x1). |
| `src/volition/vlib/code/cfile/keen_cfile.cpp:8` | Keen_cf_open_file does not return its pool entry when the keen open fails. |
| `src/volition/vlib/code/cfile/keen_cfile.cpp:9` | Keen_cf_seek with CF_SEEK_END seeks to size - offset; keen_cf_seek_packfile ignores the seek mode (offset from the start of the entry). |
| `src/volition/vlib/code/cfile/keen_cfile.cpp:11` | Keen_cf_read_compressed_chain reads the compressed data in blocks (half the data rounded up to 2 KB below 256 KB, 128 KB above) into two alternating buffers and inflates the previous block after reading the next one. With an async file the ... |
| `src/volition/vlib/code/cfile/packfile.cpp:6` | The packfile hash table uses linear probing from hash % (size - 103) up to the end of the table, without wrapping. A lookup returns the last entry with the same name before the first empty slot. Removing an entry empties its slot, which ... |
| `src/volition/vlib/code/cfile/packfile.cpp:9` | Packfile_add counts the packfile even when reading its header fails. |
| `src/volition/vlib/code/cfile/packfile.cpp:10` | Packfile_temp_find_data returns the entry's data from the last temp packfile, not from the one the entry was found in (the same when there is only one). |
| `src/volition/vlib/code/cfile/packfile.cpp:12` | V_packfile_read_and_callback leaves its temp packfile open and stops at the first name without an extension. |
| `src/volition/vlib/code/cfile/packfile.cpp:13` | Packfile_read_condensed (by name) closes the file again after a failed header read already closed it; the second close fails harmlessly. |
| `src/volition/vlib/code/cfile/vdir.cpp:7` | Vdir_restore fails for a version 3+ header without extra header bytes (it reads them with cf_read and treats 0 bytes read as an error). |
| `src/volition/vlib/code/cfile/vdir.cpp:9` | Vdir_find_ext_path compares the directory of every candidate, also for entries without a directory. |
| `src/volition/vlib/code/input/input.cpp:461` | The button's deadzone is set, although callers pass joy ids too. |
| `src/volition/vlib/code/math/md5.cpp:4` | Rfg.exe and the other way round. Original bugs kept: - The working values a, b, c, d are not reloaded from the digest for each block: they run on from block to block, and the digest (starting at zero) only sums them. - The fourth round's ... |
| `src/volition/vlib/code/math/md5.cpp:112` | See the file comment |
| `src/volition/vlib/code/math/rand.cpp:9` | Fl_rand, fl_rand_range and fl_rand_chance wrap the table index with == 2048, the others with > 2047. |
| `src/volition/vlib/code/math/rand.cpp:10` | Bool_rand is false only for draws below 0x7FFFFFFF, so it is true slightly more often than false. |
| `src/volition/vlib/code/memory/async_mempool.cpp:6` | Alloc ignores the alignment and advances the position even when the pool is full, so a failed allocation leaves current_pos past max_size until clear. |
| `src/volition/vlib/code/memory/dynamic_mempool.cpp:8` | Space_max does not set the MEMORYSTATUSEX length before GlobalMemoryStatusEx (so the call fails and the result is whatever the stack held). Here the length is set so the result is defined. |
| `src/volition/vlib/code/memory/dynamic_mempool.cpp:10` | Realloc and free do not update total_alloced. |
| `src/volition/vlib/code/memory/page_mempool.cpp:13` | Clear() writes the first header through get_header(0) even when the pool has no pages (a null write). |
| `src/volition/vlib/code/memory/page_mempool.cpp:14` | The alignment padding is computed from the pool start, not from the run that is chosen, so it is only right for alignments up to the page size (get_align_page_count handles the larger ones by skipping whole pages). |
| `src/volition/vlib/code/memory/split_memblk.cpp:8` | Alloc keeps the whole block when less than 4 KB + 1 byte would be left over. |
| `src/volition/vlib/code/memory/split_memblk.cpp:9` | Remove() of the root with both children returns the new root with its old parent still in `top`. |
| `src/volition/vlib/code/memory/split_memblk.cpp:10` | Free() returns the merged previous block after returning `this` to the pool. |
| `src/volition/vlib/code/memory/split_memmgr.cpp:15` | The tiny bucket array is allocated from the first bucket's hash_pool. |
| `src/volition/vlib/code/memory/split_memmgr.cpp:16` | When a tiny bucket accepts a request but its alloc fails, the large blocks are tried. |
| `src/volition/vlib/code/memory/split_memmgr.cpp:17` | Space_free does not count alignment padding inside large blocks. |
| `src/volition/vlib/code/memory/split_page_mempool.cpp:9` | Alloc aligns the run start with the requested align (get_align_page_count) but computes the in-page offset from max(align, def_align); new free-run headers get an uninitialised wasted_space in the original (0 here). |
| `src/volition/vlib/code/memory/split_page_mempool.cpp:11` | Realloc never advances when the next free run lies above the block's end: the loop spins forever (checked in the disassembly at 0x005CBBD5..0x005CBBE9). |
| `src/volition/vlib/code/memory/split_page_mempool.cpp:13` | Shrink_to_fit links the freed tail behind the first free run even when it lies before it, writes through a null pointer when there is no free run, and drops the tail when it lies after the last free run. |
| `src/volition/vlib/code/memory/split_page_mempool.cpp:15` | Clear() with a full header hash leaves no free run but sets free_list_count to 1. |
| `src/volition/vlib/code/memory/split_page_mempool.cpp:16` | Mark, restore_to_mark, alloc_all, pad_to_page and release_bytes(unsigned int) are not supported (log only). |
| `src/volition/vlib/code/os/scheduler.cpp:8` | Scheduler_q_job queues nothing for ccjt_any and ccjt_sim_main: such a job is taken from the free pool and never runs (its output trigger is never fired). No caller in rfg.exe adds one. |
| `src/volition/vlib/code/os/scheduler.cpp:10` | The calling thread's controller (scheduler_relinquish_cpu, _until_event, scheduler_get_processor_number) is 0 for the sim thread, 1 for the d3d thread, 1 for the culling thread and 0 for every other thread (the stage 2 thread, the boot ... |
| `src/volition/vlib/code/os/scheduler.cpp:13` | Controllers 8-15 are never initialized: scheduler_get_controller/_thread_id return them as they are. |
| `src/volition/vlib/code/os/scheduler.cpp:14` | Scheduler_in_render_thread_job reads the current job of a controller without checking it. |
| `src/volition/vlib/code/os/scheduler.cpp:15` | The free pools are not checked: scheduler_add_job, scheduler_get_event_trigger and scheduler_get_scheduling_trigger write through a null node when their pool is empty. |
| `src/volition/vlib/code/os/scheduler.cpp:17` | The atomic counters of scheduling_trigger are interlocked (LOCK prefixed in rfg.exe); num_inputs is a plain increment. |
| `src/volition/vlib/code/os/thread.cpp:7` | Thread_finished reports "finished" for a handle whose slot was reused, and returns 0xDEADBEEF as the exit code when the exit ring no longer holds the handle; thread_wait_until_done does the same. |
| `src/volition/vlib/code/os/thread.cpp:9` | Set_thread_priority / get_thread_priority / get_vlib_thread_priority_from_system_priority are constant in this build. |
| `src/volition/vlib/code/os/vconsole.cpp:10` | Console_process_gr's key repeat reads Key_states with the typed key code, modifier bits (0x1000 and up) included. Deviation: codes past the 256 keys count as released here (rfg.exe reads unrelated memory there). |
| `src/volition/vlib/code/os/vconsole.cpp:12` | Entering a command compares it with Command_history[Command_history_last] before checking that the history has an entry. Deviation: an empty history counts as a different command here (rfg.exe reads the memory before it). |
| `src/volition/vlib/code/os/vconsole.cpp:14` | The "!prefix" history recall and the history search compare case-insensitively up to the shorter length. |
| `src/volition/vlib/code/parse/parse.cpp:9` | Errors are only printed; parsing carries on from where it was. The list parsers rewind to a bad element after reporting it, so a bad element in an int/flag list repeats forever. |
| `src/volition/vlib/code/parse/parse.cpp:11` | Parse_string and parse_string_alphanumeric copy the whole token even when it does not fit (parse_string reports it first); parse_xml_required/optional_string copy the element text without a bound when the close tag is found. |
| `src/volition/vlib/code/parse/parse.cpp:13` | Parse_skip_to compares the position against size, which open_from_mem_no_copy does not set. |
| `src/volition/vlib/code/parse/parse.cpp:14` | Parse_optional_string allows white space after the opening quote but steps over the match as if there was none. |
| `src/volition/vlib/code/parse/parse.cpp:15` | Error() reads 40 bytes from the current position even past the end of the text. |
| `src/volition/vlib/code/parse/xml_table_tools.cpp:7` | Parse_uint accepts decimal or 0x-prefixed hex and stops at the first other character; parse_float builds the value from the integer part and the decimal digits after '.' (no exponent, no hex). |
| `src/volition/vlib/code/parse/xml_table_tools.cpp:9` | Xml_table_get_int_list, _float_list and _string_list do not check list_max. |
| `src/volition/vlib/code/parse/xml_table_tools.cpp:10` | Xml_table_get_flags treats a null entry in the flag list as the end of the list, without a warning for the name. |
| `src/volition/vlib/code/util/fastxml.cpp:6` | Tags have no attributes. A tag name runs to the first '>' or whitespace; after whitespace only "/>" (empty element) or '>' may follow. In that case the '>' is not consumed and is read as text. "<a/>" is not an empty element: its name is ... |
| `src/volition/vlib/code/util/fastxml.cpp:9` | Text runs keep their inner whitespace and lose the trailing whitespace. While parsing, xml_element::text of an element holds a list of text nodes (name = the text, chained through next): a comment between two text runs starts a new node. ... |
| `src/volition/vlib/code/util/fastxml.cpp:13` | A second top-level element replaces the first one as the root. |
| `src/volition/vlib/code/util/fastxml.cpp:14` | Close tags are matched case-insensitively. |
| `src/volition/vlib/code/util/handle_manager.inl:8` | The wrap to bin 0 skips the "back at the start" check, so a full manager whose search started in bin 0 never returns. |
| `src/volition/vlib/code/util/string_pool.cpp:18` | The capacity check of the wide add/add_unique counts characters, not bytes (current_pos + 1 + len against max_size), so a wide string can be written past the end of the pool. |
| `src/volition/vlib/code/util/string_pool.cpp:20` | Clear() rewinds through the virtual restore_to_mark, which for string_pool goes back to mark_pos, not to 0. |
| `src/volition/vlib/code/util/string_pool.cpp:21` | Add_string grows the string pool when space_free (end_pos - current_pos) is short, but only max_size and pool change: end_pos stays, so every later add_string reallocates the pool again (the old pools are not freed, and the hash entries of ... |
| `src/volition/vlib/code/util/unicode_tools.cpp:188` | A size of 0 writes dest[-1] |
| `src/volition/vlib/code/util/unicode_tools.cpp:272` | A size of 0 writes before the buffer. |
