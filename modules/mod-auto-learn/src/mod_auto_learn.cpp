/*
 * ZoeCore Auto Learn
 * Criado a partir do antigo mod-npc-learnspell-master.
 */

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Define.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "WorldSession.h"

#include <ctime>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

namespace
{
    std::unordered_map<uint32, uint32> PendingAutoLearn;

    static uint32 const WarriorSpells[] =
    {
        3127, 750, 264, 5011, 15590, 266, 227, 200, 674, 199, 33388, 33391, 34090, 34091, 54197, 100,
        6178, 11578, 772, 6546, 6547, 6548, 11572, 11573, 11574, 25208, 46845, 47465, 6343, 8198, 8204, 8205,
        11580, 11581, 25264, 47501, 47502, 1715, 284, 285, 1608, 11564, 11565, 11566, 11567, 25286, 29707, 30324,
        47449, 47450, 7384, 694, 20230, 64382, 57755, 21551, 21552, 21553, 25248, 30330, 47485, 47486, 6673, 5242,
        6192, 11549, 11550, 11551, 25289, 2048, 47436, 34428, 1160, 6190, 11554, 11555, 11556, 25202, 25203, 47437,
        845, 7369, 11608, 11609, 20569, 25231, 47519, 47520, 5246, 5308, 20658, 20660, 20661, 20662, 25234, 25236,
        47470, 47471, 1161, 2458, 20252, 1464, 8820, 11604, 11605, 25241, 25242, 47474, 47475, 18499, 1680, 6552,
        1719, 469, 47439, 47440, 55694, 2687, 71, 72, 6572, 6574, 7379, 11600, 11601, 25288, 25269, 30357,
        57823, 2565, 676, 12678, 871, 7386, 355, 23922, 23923, 23924, 23925, 25258, 30356, 47487, 47488, 23920,
        3411, 30016, 30022, 47497, 47498
    };

    static uint32 const PaladinSpells[] =
    {
        3127, 750, 196, 197, 199, 200, 33388, 33391, 34090, 34091, 54197, 31821, 53563, 19742, 19850, 19852,
        19853, 19854, 25290, 27142, 48935, 48936, 4987, 19746, 26573, 20116, 20922, 20923, 20924, 27173, 48818, 48819,
        20216, 31842, 54428, 879, 5614, 5615, 10312, 10313, 10314, 27138, 48800, 48801, 19750, 19939, 19940, 19941,
        19942, 19943, 27137, 48784, 48785, 25894, 25918, 27143, 48937, 48938, 639, 647, 1026, 1042, 3472, 10328,
        10329, 25292, 27135, 27136, 48781, 48782, 2812, 10318, 27139, 48816, 48817, 633, 2800, 10310, 27154, 48788,
        1152, 7328, 10322, 10324, 20772, 20773, 48949, 48950, 53601, 20165, 21084, 20166, 5502, 10326, 20929, 20930,
        27174, 33072, 48824, 48825, 20217, 465, 10290, 643, 10291, 1032, 10292, 10293, 27149, 48941, 48942, 19752,
        498, 642, 19891, 19899, 19900, 27153, 48947, 19888, 19897, 19898, 27152, 48945, 25898, 853, 5588, 5589,
        10308, 1044, 1022, 5599, 10278, 62124, 6940, 1038, 31789, 25780, 20164, 19876, 19895, 19896, 27151, 48943,
        53600, 61411, 20927, 20928, 27179, 48951, 48952, 32699, 32700, 48826, 48827, 25899, 31884, 19740, 19834, 19835,
        19836, 19837, 19838, 25291, 27140, 48931, 48932, 32223, 25782, 25916, 27141, 48933, 48934, 24275, 24274, 24239,
        27180, 48805, 48806, 53407, 20271, 53408, 7294, 10298, 10299, 10300, 10301, 27150, 54043
    };

    static uint32 const HunterSpells[] =
    {
        3127, 8737, 264, 5011, 1180, 266, 15590, 200, 227, 2567, 202, 674, 33388, 33391, 34090, 34091,
        54197, 13161, 5118, 61846, 61847, 13165, 14318, 14319, 14320, 14321, 14322, 25296, 27044, 13163, 13159, 34074,
        20043, 20190, 27045, 49071, 1462, 883, 62757, 2641, 6197, 1002, 6991, 34026, 53271, 136, 3111, 3661,
        3662, 13542, 13543, 13544, 27046, 48989, 48990, 982, 1513, 14326, 14327, 1515, 3044, 14281, 14282, 14283,
        14284, 14285, 14286, 14287, 27019, 49044, 49045, 5116, 20736, 1543, 1130, 14323, 14324, 14325, 53338, 53351,
        61005, 61006, 2643, 14288, 14289, 14290, 25294, 27021, 49047, 49048, 3045, 3043, 1978, 13549, 13550, 13551,
        13552, 13553, 13554, 13555, 25295, 27016, 49000, 49001, 56641, 34120, 49051, 49052, 19801, 3034, 1510, 14294,
        14295, 27022, 58431, 58434, 20900, 20901, 20902, 20903, 20904, 27065, 49049, 49050, 19263, 781, 13813, 14316,
        14317, 27025, 49066, 49067, 5384, 60192, 1499, 14310, 14311, 13809, 13795, 14302, 14303, 14304, 14305, 27023,
        49055, 49056, 34477, 1495, 14269, 14270, 14271, 36916, 53339, 2973, 14260, 14261, 14262, 14263, 14264, 14265,
        14266, 27014, 48995, 48996, 34600, 1494, 19878, 19879, 19880, 19882, 19885, 19883, 19884, 2974, 20909, 20910,
        27067, 48998, 48999, 24132, 24133, 27068, 49011, 49012, 60051, 60052, 60053, 63668, 63669, 63670, 63671, 63672
    };

    static uint32 const RogueSpells[] =
    {
        3127, 1804, 196, 264, 5011, 15590, 266, 198, 201, 33388, 33391, 34090, 34091, 54197, 8676, 8724,
        8725, 11267, 11268, 11269, 27441, 48689, 48690, 48691, 1833, 26679, 48673, 48674, 51722, 32645, 32684, 57992,
        57993, 6760, 6761, 6762, 8623, 8624, 11299, 11300, 31016, 26865, 48667, 48668, 8647, 703, 8631, 8632,
        8633, 11289, 11290, 26839, 26884, 48675, 48676, 408, 8643, 1943, 8639, 8640, 11273, 11274, 11275, 26867,
        48671, 48672, 5171, 6774, 34411, 34412, 34413, 48663, 48666, 53, 2589, 2590, 2591, 8721, 11279, 11280,
        11281, 25300, 26863, 48656, 48657, 1776, 1766, 5938, 51723, 1752, 1757, 1758, 1759, 1760, 8621, 11293,
        11294, 26861, 26862, 48637, 48638, 5277, 26669, 2983, 8696, 11305, 1966, 6768, 8637, 11303, 25302, 27448,
        48658, 48659, 1784, 921, 1725, 1842, 2094, 31224, 57934, 2836, 1860, 6770, 2070, 11297, 51724, 1856,
        1857, 26889, 17347, 17348, 26864, 48660
    };

    static uint32 const PriestSpells[] =
    {
        1180, 33388, 33391, 34090, 34091, 54197, 527, 988, 14752, 14818, 14819, 27841, 25312, 48073, 6346, 588,
        7128, 602, 1006, 10951, 10952, 25431, 48040, 48168, 1706, 8129, 32375, 1243, 1244, 1245, 2791, 10937,
        10938, 25389, 48161, 17, 592, 600, 3747, 6065, 6066, 10898, 10899, 10900, 10901, 25217, 25218, 48065,
        48066, 21562, 21564, 25392, 48162, 27681, 32999, 48074, 9484, 9485, 10955, 53006, 53007, 552, 32546, 48119,
        48120, 528, 64843, 2061, 9472, 9473, 9474, 10915, 10916, 10917, 25233, 25235, 48070, 48071, 2060, 10963,
        10964, 10965, 25314, 25210, 25213, 48062, 48063, 2054, 2055, 6063, 6064, 14914, 15262, 15263, 15264, 15265,
        15266, 15267, 15261, 25384, 48134, 48135, 15237, 15430, 15431, 27799, 27800, 27801, 25331, 48077, 48078, 64901,
        2050, 2052, 2053, 596, 996, 10960, 10961, 25316, 25308, 48072, 33076, 48112, 48113, 139, 6074, 6075,
        6076, 6077, 6078, 10927, 10928, 10929, 25315, 25221, 25222, 48067, 48068, 2006, 2010, 10880, 10881, 20770,
        25435, 48171, 585, 591, 598, 984, 1004, 6060, 10933, 10934, 25363, 25364, 48122, 48123, 27870, 27871,
        28275, 48086, 48087, 19238, 19240, 19241, 19242, 19243, 25437, 48172, 48173, 34863, 34864, 34865, 34866, 48088,
        48089, 2944, 19276, 19277, 19278, 19279, 19280, 25467, 48299, 48300, 586, 8092, 8102, 8103, 8104, 8105,
        8106, 10945, 10946, 10947, 25372, 25375, 48126, 48127, 605, 48045, 53023, 453, 2096, 10909, 27683, 39374,
        48170, 8122, 8124, 10888, 10890, 976, 10957, 10958, 25433, 48169, 32379, 32996, 48157, 48158, 589, 594,
        970, 992, 2767, 10892, 10893, 10894, 25367, 25368, 48124, 48125, 34433, 17311, 17312, 17313, 17314, 18807,
        25387, 48155, 48156, 34916, 34917, 48159, 48160
    };

    static uint32 const DeathKnightSpells[] =
    {
        198, 199, 33388, 33391, 34090, 34091, 54197, 48778, 50842, 48721, 49939, 49940, 49941, 49926, 49927, 49928,
        49929, 49930, 47476, 45529, 56222, 48743, 48263, 47528, 45524, 49896, 49903, 49904, 49909, 49020, 51423, 51424,
        51425, 3714, 48792, 57330, 57623, 56815, 47568, 49998, 49999, 45463, 49923, 49924, 46584, 43265, 49936, 49937,
        49938, 49917, 49918, 49919, 49920, 49921, 49892, 49893, 49894, 49895, 48707, 48265, 61999, 42650, 53428, 53341,
        53331, 53343, 54447, 53342, 54446, 53323, 53344, 70164, 62158, 55258, 55259, 55260, 55261, 55262, 51416, 51417,
        51418, 51419, 55268, 51409, 51410, 51411, 51325, 51326, 51327, 51328, 55265, 55270, 55271
    };

    static uint32 const ShamanSpells[] =
    {
        8737, 196, 1180, 15590, 197, 199, 33388, 33391, 34090, 34091, 54197, 66843, 66842, 66844, 421, 930,
        2860, 10605, 25439, 25442, 49270, 49271, 8042, 8044, 8045, 8046, 10412, 10413, 10414, 25454, 49230, 49231,
        2484, 2894, 1535, 8498, 8499, 11314, 11315, 25546, 25547, 61649, 61657, 8050, 8052, 8053, 10447, 10448,
        29228, 25457, 49232, 49233, 8056, 8058, 10472, 10473, 25464, 49235, 49236, 51514, 51505, 60043, 403, 529,
        548, 915, 943, 6041, 10391, 10392, 15207, 15208, 25448, 25449, 49237, 49238, 8190, 10585, 10586, 10587,
        25552, 58731, 58734, 370, 8012, 3599, 6363, 6364, 6365, 10437, 10438, 25533, 58699, 58703, 58704, 5730,
        6390, 6391, 6392, 10427, 10428, 25525, 58580, 58581, 58582, 57994, 57720, 57721, 57722, 59156, 59158, 59159,
        556, 2062, 6196, 8184, 10537, 10538, 25563, 58737, 58739, 8227, 8249, 10526, 16387, 25557, 58649, 58652,
        58656, 8024, 8027, 8030, 16339, 16341, 16342, 25489, 58785, 58789, 58790, 8181, 10478, 10479, 25560, 58741,
        58745, 8033, 8038, 10456, 16355, 16356, 25500, 58794, 58795, 58796, 2645, 8177, 324, 325, 905, 945,
        8134, 10431, 10432, 25469, 25472, 49280, 49281, 10595, 10600, 10601, 25574, 58746, 58749, 8017, 8018, 8019,
        10399, 6495, 8071, 8154, 8155, 10406, 10407, 10408, 25508, 25509, 58751, 58753, 8075, 8160, 8161, 10442,
        25361, 25528, 57622, 58643, 131, 546, 8512, 8232, 8235, 10486, 16362, 25505, 58801, 58803, 58804, 3738,
        2008, 20609, 20610, 20776, 20777, 25590, 49277, 1064, 10622, 10623, 25422, 25423, 55458, 55459, 8170, 526,
        51730, 51988, 51991, 51992, 51993, 51994, 5394, 6375, 6377, 10462, 10463, 25567, 58755, 58756, 58757, 331,
        332, 547, 913, 939, 959, 8005, 10395, 10396, 25357, 25391, 25396, 49272, 49273, 8004, 8008, 8010,
        10466, 10467, 10468, 25420, 49275, 49276, 5675, 10495, 10496, 10497, 25570, 58771, 58773, 58774, 20608, 36936,
        8143, 52127, 52129, 52131, 52134, 52136, 52138, 24398, 33736, 57960, 61299, 61300, 61301, 32593, 32594, 49283,
        49284, 32182, 2825
    };

    static uint32 const MageSpells[] =
    {
        1180, 201, 33388, 33391, 34090, 34091, 54197, 1008, 8455, 10169, 10170, 27130, 33946, 43017, 30451, 42894,
        42896, 42897, 23028, 27127, 43002, 1449, 8437, 8438, 8439, 10201, 10202, 27080, 27082, 42920, 42921, 1459,
        1460, 1461, 10156, 10157, 27126, 42995, 5143, 5144, 5145, 8416, 8417, 10211, 10212, 25345, 27075, 38699,
        38704, 42843, 42846, 1953, 587, 597, 990, 6129, 10144, 10145, 28612, 33717, 759, 3552, 10053, 10054,
        27101, 42985, 42955, 42956, 5504, 5505, 5506, 6127, 10138, 10139, 10140, 37420, 27090, 2139, 604, 8450,
        8451, 10173, 10174, 33944, 43015, 12051, 66, 6117, 22782, 22783, 27125, 43023, 43024, 1463, 8494, 8495,
        10191, 10192, 10193, 27131, 43019, 43020, 55342, 118, 12824, 12825, 12826, 28272, 53142, 475, 43987, 58659,
        130, 30449, 53140, 44780, 44781, 10059, 11419, 32266, 11416, 33691, 49360, 3561, 32271, 49359, 3565, 33690,
        3562, 11417, 35717, 32267, 49361, 11420, 11418, 3567, 35715, 32272, 49358, 3566, 3563, 2136, 2137, 2138,
        8412, 8413, 10197, 10199, 27078, 27079, 42872, 42873, 543, 8457, 8458, 10223, 10225, 27128, 43010, 133,
        143, 145, 3140, 8400, 8401, 8402, 10148, 10149, 10150, 10151, 25306, 27070, 38692, 42832, 42833, 2120,
        2121, 8422, 8423, 10215, 10216, 27086, 42925, 42926, 44614, 47610, 30482, 43045, 43046, 2948, 8444, 8445,
        8446, 10205, 10206, 10207, 27073, 27074, 42858, 42859, 12505, 12522, 12523, 12524, 12525, 12526, 18809, 27132,
        33938, 42890, 42891, 13018, 13019, 13020, 13021, 27133, 33933, 42944, 42945, 33041, 33042, 33043, 42949, 42950,
        55359, 55360, 10, 6141, 8427, 10185, 10186, 10187, 27085, 42939, 42940, 120, 8492, 10159, 10160, 10161,
        27087, 42930, 42931, 168, 7300, 7301, 122, 865, 6131, 10230, 27088, 42917, 6143, 8461, 8462, 10177,
        28609, 32796, 43012, 116, 205, 837, 7322, 8406, 8407, 8408, 10179, 10180, 10181, 25304, 27071, 27072,
        38697, 42841, 42842, 7302, 7320, 10219, 10220, 27124, 43008, 45438, 30455, 42913, 42914, 13031, 13032, 13033,
        27134, 33405, 43038, 43039
    };

    static uint32 const WarlockSpells[] =
    {
        201, 33388, 33391, 34090, 34091, 54197, 5784, 23161, 172, 6222, 6223, 7648, 11671, 11672, 25311, 27216,
        47812, 47813, 980, 1014, 6217, 11711, 11712, 11713, 27218, 47863, 47864, 603, 30910, 47867, 1490, 11721,
        11722, 27228, 47865, 1714, 11719, 702, 1108, 6205, 7646, 11707, 11708, 27224, 30909, 50511, 6789, 17925,
        17926, 27223, 47859, 47860, 689, 699, 709, 7651, 11699, 11700, 27219, 27220, 47857, 5138, 1120, 8288,
        8289, 11675, 27217, 47855, 5782, 6213, 6215, 5484, 17928, 1454, 1455, 1456, 11687, 11688, 11689, 27222,
        57946, 27243, 47835, 47836, 30404, 30405, 47841, 47843, 59161, 59163, 59164, 18937, 18938, 27265, 59092, 710,
        18647, 59671, 6366, 17951, 17952, 17953, 27250, 60219, 60220, 6201, 6202, 5699, 11729, 11730, 27230, 47871,
        47878, 693, 20752, 20755, 20756, 20757, 27238, 47884, 2362, 17727, 17728, 28172, 47886, 47888, 706, 1086,
        11733, 11734, 11735, 27260, 47793, 47889, 54785, 687, 696, 48018, 48020, 132, 1098, 11725, 11726, 61191,
        126, 28176, 28189, 47892, 47893, 755, 3698, 3699, 3700, 11693, 11694, 11695, 27259, 47856, 50589, 1122,
        18540, 29893, 58887, 698, 5500, 6229, 11739, 11740, 28610, 47890, 47891, 29858, 691, 688, 712, 697,
        5697, 1949, 11683, 11684, 27213, 47823, 348, 707, 1094, 2941, 11665, 11667, 11668, 25309, 27215, 47810,
        47811, 29722, 32231, 47837, 47838, 5740, 6219, 11677, 11678, 27212, 47819, 47820, 5676, 17919, 17920, 17921,
        17922, 17923, 27210, 30459, 47814, 47815, 686, 695, 705, 1088, 1106, 7641, 11659, 11660, 11661, 25307,
        27209, 47808, 47809, 47897, 61290, 6353, 17924, 27211, 30545, 47824, 47825, 18867, 18868, 18869, 18870, 18871,
        27263, 30546, 47826, 47827, 30413, 30414, 47846, 47847, 59170, 59171, 59172
    };

    static uint32 const DruidSpells[] =
    {
        15590, 199, 200, 33388, 33391, 34090, 34091, 54197, 22812, 33786, 339, 1062, 5195, 5196, 9852, 9853,
        26989, 53308, 770, 2637, 18657, 18658, 16914, 17401, 17402, 27012, 48467, 29166, 8921, 8924, 8925, 8926,
        8927, 8928, 8929, 9833, 9834, 9835, 26987, 26988, 48462, 48463, 16689, 16810, 16811, 16812, 16813, 17329,
        27009, 53312, 2908, 8955, 9901, 26995, 2912, 8949, 8950, 8951, 9875, 9876, 25298, 26986, 48464, 48465,
        467, 782, 1075, 8914, 9756, 9910, 26992, 53307, 5176, 5177, 5178, 5179, 5180, 6780, 8905, 9912,
        26984, 26985, 48459, 48461, 24974, 24975, 24976, 24977, 27013, 48468, 53223, 53225, 53226, 61384, 53199, 53200,
        53201, 1066, 8983, 768, 5209, 48570, 48575, 33357, 48560, 9634, 5229, 16857, 20719, 48577, 33943, 22842,
        6795, 48568, 49802, 48480, 49803, 5215, 48574, 48579, 49800, 62600, 52610, 48572, 48562, 62078, 50213, 5225,
        783, 33986, 33982, 33983, 48565, 48566, 33987, 48563, 48564, 2893, 21849, 21850, 26991, 48470, 5185, 5186,
        5187, 5188, 5189, 6778, 8903, 9758, 9888, 9889, 25297, 26978, 26979, 48377, 48378, 33763, 48450, 48451,
        1126, 5232, 6756, 5234, 8907, 9884, 9885, 26990, 48469, 50464, 20484, 20739, 20742, 20747, 20748, 26994,
        48477, 8936, 8938, 8939, 8940, 8941, 9750, 9856, 9857, 9858, 26980, 48442, 48443, 774, 1058, 1430,
        2090, 2091, 3627, 8910, 9839, 9840, 9841, 25299, 26981, 26982, 48440, 48441, 2782, 50769, 50768, 50767,
        50766, 50765, 50764, 50763, 740, 8918, 9862, 9863, 26983, 48446, 48447, 53248, 53249, 53251
    };

    static uint32 const PaladinAllianceSpells[] =
    {
        31801, 13819, 23214
    };

    static uint32 const PaladinHordeSpells[] =
    {
        53736, 34769, 34767
    };

    static uint32 const WeaponSkillSpells[] =
    {
        196, 197, 198, 199, 200, 201, 202, 227, 264, 266, 674, 750, 1180, 2567, 3127, 5011,
        8737, 15590
    };

    static uint32 const RidingSpells[] =
    {
        33388, 33391, 34090, 34091, 54197
    };


    struct SkillCleanupEntry
    {
        uint32 SpellId;
        uint32 SkillId;
    };

    // Lista das skills que a V3 podia ter aplicado globalmente.
    // Usada para limpar spells/skills inválidas quando o player troca Dual Spec/Talents.
    static SkillCleanupEntry const ExtraWeaponSkillCleanup[] =
    {
        { 196, 44 },    // One-Handed Axes
        { 197, 172 },   // Two-Handed Axes
        { 198, 54 },    // One-Handed Maces
        { 199, 160 },   // Two-Handed Swords
        { 200, 229 },   // Polearms
        { 201, 43 },    // One-Handed Swords
        { 202, 55 },    // Two-Handed Maces
        { 227, 136 },   // Staves
        { 264, 45 },    // Bows
        { 266, 46 },    // Guns
        { 674, 118 },   // Dual Wield
        { 750, 293 },   // Plate Mail
        { 1180, 173 },  // Daggers
        { 2567, 176 },  // Thrown
        { 3127, 95 },   // Parry
        { 5011, 226 },  // Crossbows
        { 8737, 413 },  // Mail
        { 15590, 473 }  // Fist Weapons
    };

    bool IsAllowedExtraSkillSpell(Player* player, uint32 spellId)
    {
        if (!player)
            return false;

        switch (player->getClass())
        {
            case CLASS_WARRIOR:
                // Warrior é a classe mais ampla; manter tudo que a V3 poderia ter dado.
                return true;

            case CLASS_PALADIN:
                switch (spellId)
                {
                    case 196: case 197: case 198: case 199: case 200: case 201:
                    case 202: case 3127: case 750:
                        return true;
                    default:
                        return false;
                }

            case CLASS_HUNTER:
                switch (spellId)
                {
                    case 196: case 197: case 199: case 200: case 201: case 227:
                    case 264: case 266: case 674: case 1180: case 2567: case 3127:
                    case 5011: case 8737: case 15590:
                        return true;
                    default:
                        return false;
                }

            case CLASS_ROGUE:
                switch (spellId)
                {
                    case 196: case 198: case 201: case 264: case 266: case 674:
                    case 1180: case 2567: case 3127: case 5011: case 15590:
                        return true;
                    default:
                        return false;
                }

            case CLASS_PRIEST:
                switch (spellId)
                {
                    case 198: case 227: case 1180:
                        return true;
                    default:
                        return false;
                }

            case CLASS_DEATH_KNIGHT:
                switch (spellId)
                {
                    case 196: case 197: case 198: case 199: case 200: case 201:
                    case 202: case 674: case 3127: case 750:
                        return true;
                    default:
                        return false;
                }

            case CLASS_SHAMAN:
                switch (spellId)
                {
                    case 196: case 197: case 198: case 202: case 227: case 674:
                    case 1180: case 8737: case 15590:
                        return true;
                    default:
                        return false;
                }

            case CLASS_MAGE:
                switch (spellId)
                {
                    case 201: case 227: case 1180:
                        return true;
                    default:
                        return false;
                }

            case CLASS_WARLOCK:
                switch (spellId)
                {
                    case 201: case 227: case 1180:
                        return true;
                    default:
                        return false;
                }

            case CLASS_DRUID:
                switch (spellId)
                {
                    case 198: case 200: case 202: case 227: case 1180: case 15590:
                        return true;
                    default:
                        return false;
                }

            default:
                return false;
        }
    }

    void CleanupInvalidExtraWeaponSkills(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("AutoLearn.InvalidSkillCleanup.Enable", true))
            return;

        for (SkillCleanupEntry const& entry : ExtraWeaponSkillCleanup)
        {
            if (IsAllowedExtraSkillSpell(player, entry.SpellId))
                continue;

            if (entry.SpellId && player->HasSpell(entry.SpellId))
                player->removeSpell(entry.SpellId, false, false);

            if (entry.SkillId && player->GetSkillValue(entry.SkillId) > 0)
                player->SetSkill(entry.SkillId, 0, 0, 0);
        }
    }

    bool AutoLearnEnabled()
    {
        return sConfigMgr->GetOption<bool>("AutoLearn.Enable", true);
    }

    template <std::size_t N>
    void LearnSpellArray(Player* player, uint32 const (&spells)[N])
    {
        if (!player)
            return;

        for (uint32 spellId : spells)
        {
            if (!spellId)
                continue;

            if (!player->HasSpell(spellId))
                player->learnSpell(spellId);
        }
    }

    std::vector<uint32> ParseSpellList(std::string const& text)
    {
        std::vector<uint32> spells;
        std::stringstream stream(text);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);

            if (token.empty())
                continue;

            try
            {
                uint32 spellId = static_cast<uint32>(std::stoul(token));
                if (spellId)
                    spells.push_back(spellId);
            }
            catch (...)
            {
                continue;
            }
        }

        return spells;
    }

    void LearnMountList(Player* player, std::string const& spellList)
    {
        if (!player)
            return;

        for (uint32 spellId : ParseSpellList(spellList))
        {
            if (!player->HasSpell(spellId))
                player->learnSpell(spellId);
        }
    }

    void LearnConfiguredMountSpells(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("AutoLearn.Mounts.Enable", true))
            return;

        // Lista comum nova.
        LearnMountList(player, sConfigMgr->GetOption<std::string>("AutoLearn.Mounts.CommonSpellList", "72286"));

        // Compatibilidade com V3.
        LearnMountList(player, sConfigMgr->GetOption<std::string>("AutoLearn.Mounts.SpellList", ""));

        // Lista por facção.
        if (player->GetTeamId() == TEAM_ALLIANCE)
            LearnMountList(player, sConfigMgr->GetOption<std::string>("AutoLearn.Mounts.AllianceSpellList", ""));
        else
            LearnMountList(player, sConfigMgr->GetOption<std::string>("AutoLearn.Mounts.HordeSpellList", ""));
    }

    void CastDualSpecSpells(Player* player)
    {
        // V6: Dual Spec fica desativado por padrão na config.
        // Mantemos a função apenas para quem quiser reativar manualmente no .conf.
        if (!player || !sConfigMgr->GetOption<bool>("AutoLearn.DualSpec.Enable", false))
            return;

        if (sConfigMgr->GetOption<bool>("AutoLearn.DualSpec.CastSpells", false))
        {
            player->CastSpell(player, 63680, true);
            player->CastSpell(player, 63624, true);
        }
        else
        {
            if (!player->HasSpell(63680))
                player->learnSpell(63680);

            if (!player->HasSpell(63624))
                player->learnSpell(63624);
        }
    }

    void LearnCommonSkills(Player* player)
    {
        if (!player)
            return;

        // Importante:
        // Não ensine WeaponSkillSpells para todas as classes por padrão.
        // A core valida race/class e remove skills inválidas, gerando logs como:
        // "has spell (...) which is invalid for the race/class combination".
        //
        // As skills válidas de cada classe já estão dentro das listas LegacyNpcSpellLists
        // extraídas do NPC antigo, então aqui só usamos a lista global se for forçada na config.
        if (sConfigMgr->GetOption<bool>("AutoLearn.WeaponSkills.ExtraAllClasses.Enable", false))
            LearnSpellArray(player, WeaponSkillSpells);

        if (sConfigMgr->GetOption<bool>("AutoLearn.Riding.Enable", true))
            LearnSpellArray(player, RidingSpells);

        LearnConfiguredMountSpells(player);

        if (sConfigMgr->GetOption<bool>("AutoLearn.MaxSkills.Enable", true))
            player->UpdateSkillsToMaxSkillsForLevel();
    }

    void LearnClassSpells(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("AutoLearn.ClassSpells.Enable", true))
            return;

        if (!sConfigMgr->GetOption<bool>("AutoLearn.LegacyNpcSpellLists.Enable", true))
            return;

        switch (player->getClass())
        {
            case CLASS_WARRIOR:
                LearnSpellArray(player, WarriorSpells);
                break;
            case CLASS_PALADIN:
                if (player->GetTeamId() == TEAM_ALLIANCE)
                    LearnSpellArray(player, PaladinAllianceSpells);
                else
                    LearnSpellArray(player, PaladinHordeSpells);
                LearnSpellArray(player, PaladinSpells);
                break;
            case CLASS_HUNTER:
                LearnSpellArray(player, HunterSpells);
                break;
            case CLASS_ROGUE:
                LearnSpellArray(player, RogueSpells);
                break;
            case CLASS_PRIEST:
                LearnSpellArray(player, PriestSpells);
                break;
            case CLASS_DEATH_KNIGHT:
                LearnSpellArray(player, DeathKnightSpells);
                break;
            case CLASS_SHAMAN:
                LearnSpellArray(player, ShamanSpells);
                break;
            case CLASS_MAGE:
                LearnSpellArray(player, MageSpells);
                break;
            case CLASS_WARLOCK:
                LearnSpellArray(player, WarlockSpells);
                break;
            case CLASS_DRUID:
                LearnSpellArray(player, DruidSpells);
                break;
            default:
                break;
        }
    }

    void LearnAllTalents(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("AutoLearn.Talents.Enable", true))
            return;

        uint32 classMask = player->getClassMask();
        uint32 passes = sConfigMgr->GetOption<uint32>("AutoLearn.Talents.Passes", 3);
        if (passes == 0)
            passes = 1;

        for (uint32 pass = 0; pass < passes; ++pass)
        {
            for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
                if (!talentInfo)
                    continue;

                TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
                if (!talentTabInfo)
                    continue;

                if ((classMask & talentTabInfo->ClassMask) == 0)
                    continue;

                uint32 spellId = 0;
                uint8 rankId = MAX_TALENT_RANK;

                for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
                {
                    if (talentInfo->RankID[rank] != 0)
                    {
                        rankId = rank;
                        spellId = talentInfo->RankID[rank];
                        break;
                    }
                }

                if (!spellId || rankId == MAX_TALENT_RANK)
                    continue;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
                if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo))
                    continue;

                player->LearnTalent(talentInfo->TalentID, rankId);
            }

            if (sConfigMgr->GetOption<bool>("AutoLearn.Talents.CastRefreshSpell", true))
            {
                uint32 refreshSpell = sConfigMgr->GetOption<uint32>("AutoLearn.Talents.RefreshSpellId", 36400);
                if (refreshSpell)
                    player->CastSpell(player, refreshSpell, true);
            }
        }

        if (sConfigMgr->GetOption<bool>("AutoLearn.Talents.ClearFreePoints", true))
            player->SetFreeTalentPoints(0);

        player->SendTalentsInfoData(false);
    }

    bool AlreadyApplied(Player* player)
    {
        if (!player)
            return true;

        if (!sConfigMgr->GetOption<bool>("AutoLearn.OnlyOnce", true))
            return false;

        uint32 version = sConfigMgr->GetOption<uint32>("AutoLearn.Version", 1);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `version` FROM `zoecore_auto_learn_state` WHERE `guid`={} AND `version` >= {}",
            player->GetGUID().GetCounter(), version);

        return bool(result);
    }

    void MarkApplied(Player* player)
    {
        if (!player || !player->GetSession())
            return;

        uint32 version = sConfigMgr->GetOption<uint32>("AutoLearn.Version", 1);

        CharacterDatabase.Execute(
            "REPLACE INTO `zoecore_auto_learn_state` (`guid`, `account_id`, `player_name`, `version`, `applied_at`) VALUES ({}, {}, '{}', {}, {})",
            player->GetGUID().GetCounter(), player->GetSession()->GetAccountId(), player->GetName(), version, uint32(time(nullptr)));
    }

    void ApplyAutoLearn(Player* player, bool manual)
    {
        if (!player || !player->GetSession() || !AutoLearnEnabled())
            return;

        uint32 minLevel = sConfigMgr->GetOption<uint32>("AutoLearn.MinLevel", 0);
        if (minLevel && player->GetLevel() < minLevel)
            return;

        // Limpa resíduos de versões antigas antes de checar OnlyOnce.
        // Isso evita logs ao trocar Dual Spec/Talents em personagens que receberam skills inválidas.
        CleanupInvalidExtraWeaponSkills(player);

        if (!manual && AlreadyApplied(player))
        {
            if (sConfigMgr->GetOption<bool>("AutoLearn.Announce.Enable", true) &&
                !sConfigMgr->GetOption<bool>("AutoLearn.OnlyOnce", true))
            {
                ChatHandler(player->GetSession()).SendSysMessage(
                    sConfigMgr->GetOption<std::string>("AutoLearn.Message.AlreadyDone", "|cff00FFFF[ZoeCore]|r Seu personagem ja recebeu o treinamento automatico.").c_str());
            }
            return;
        }

        CastDualSpecSpells(player);
        LearnCommonSkills(player);
        LearnClassSpells(player);
        LearnAllTalents(player);
        LearnCommonSkills(player);
        CleanupInvalidExtraWeaponSkills(player);

        player->SaveToDB(false, true);
        MarkApplied(player);

        if (sConfigMgr->GetOption<bool>("AutoLearn.Announce.Enable", true))
            ChatHandler(player->GetSession()).SendSysMessage(
                sConfigMgr->GetOption<std::string>("AutoLearn.Message.Done", "|cff00FFFF[ZoeCore]|r Suas spells, talentos e skills foram treinadas automaticamente.").c_str());
    }
}

using namespace Acore::ChatCommands;

class ZoeCoreAutoLearnCommandScript : public CommandScript
{
public:
    ZoeCoreAutoLearnCommandScript() : CommandScript("ZoeCoreAutoLearnCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable AutoLearnCommandTable =
        {
            { "info", HandleInfoCommand, SEC_PLAYER, Console::Yes },
            { "apply", HandleApplyCommand, SEC_PLAYER, Console::Yes },
            { "clean", HandleCleanCommand, SEC_PLAYER, Console::Yes },
            { "reset", HandleResetCommand, SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable AutoLearnBaseTable =
        {
            { "autolearn", AutoLearnCommandTable },
        };

        return AutoLearnBaseTable;
    }

    static bool HandleInfoCommand(ChatHandler* handler)
    {
        if (!handler || !handler->GetSession())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        handler->PSendSysMessage("AutoLearn Enable: {}", AutoLearnEnabled() ? "ON" : "OFF");
        handler->PSendSysMessage("OnlyOnce: {}", sConfigMgr->GetOption<bool>("AutoLearn.OnlyOnce", true) ? "ON" : "OFF");
        handler->PSendSysMessage("Version: {}", sConfigMgr->GetOption<uint32>("AutoLearn.Version", 1));
        handler->PSendSysMessage("Aplicado neste personagem: {}", AlreadyApplied(player) ? "SIM" : "NAO");
        return true;
    }

    static bool HandleApplyCommand(ChatHandler* handler)
    {
        if (!handler || !handler->GetSession())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        ApplyAutoLearn(player, true);
        return true;
    }

    static bool HandleCleanCommand(ChatHandler* handler)
    {
        if (!handler || !handler->GetSession())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        CleanupInvalidExtraWeaponSkills(player);
        player->SaveToDB(false, true);
        handler->SendSysMessage("AutoLearn: limpeza de skills/spells invalidas executada.");
        return true;
    }

    static bool HandleResetCommand(ChatHandler* handler)
    {
        if (!handler || !handler->GetSession())
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        CharacterDatabase.Execute("DELETE FROM `zoecore_auto_learn_state` WHERE `guid`={}", player->GetGUID().GetCounter());
        handler->SendSysMessage("AutoLearn resetado para seu personagem.");
        return true;
    }
};

class ZoeCoreAutoLearnPlayerScript : public PlayerScript
{
public:
    ZoeCoreAutoLearnPlayerScript() : PlayerScript("ZoeCoreAutoLearnPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !AutoLearnEnabled() || !sConfigMgr->GetOption<bool>("AutoLearn.OnLogin.Enable", true))
            return;

        uint32 delayMs = sConfigMgr->GetOption<uint32>("AutoLearn.DelayMs", 1500);
        if (delayMs == 0)
        {
            ApplyAutoLearn(player, false);
            return;
        }

        PendingAutoLearn[player->GetGUID().GetCounter()] = delayMs;
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !AutoLearnEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = PendingAutoLearn.find(guid);
        if (itr == PendingAutoLearn.end())
            return;

        if (diff >= itr->second)
        {
            PendingAutoLearn.erase(itr);
            ApplyAutoLearn(player, false);
            return;
        }

        itr->second -= diff;
    }
};

void AddZoeCoreAutoLearnScripts()
{
    new ZoeCoreAutoLearnPlayerScript();

    if (sConfigMgr->GetOption<bool>("AutoLearn.Commands.Enable", true))
        new ZoeCoreAutoLearnCommandScript();
}
