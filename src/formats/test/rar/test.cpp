#include "../utils.h"

namespace
{
  void TestBase(const Dump& etalon)
  {
    std::cout << "Testing usual archive" << std::endl;
    //0ffsets:
    //0x14
    //0x403a
    //0x6095
    //0x80ef
    //0xa10d
    //0xc129
    //0xe148
    Dump rar;
    Test::OpenFile("test_v2.rar", rar);

    const Formats::Packed::Decoder::Ptr decoder = Formats::Packed::CreateRarDecoder();
    std::map<std::string, Dump> tests;
    const uint8_t* const data = &rar[0];
    tests["-m0"] = Dump(data + 0x14, data + 0x403a);
    tests["-m1"] = Dump(data + 0x403a, data + 0x6095);
    tests["-m2"] = Dump(data + 0x6095, data + 0x80ef);
    tests["-m3"] = Dump(data + 0x80ef, data + 0xa10d);
    tests["-m4"] = Dump(data + 0xa10d, data + 0xc129);
    tests["-m5"] = Dump(data + 0xc129, data + 0xe148);
    tests["-mm"] = Dump(data + 0xe148, data + 0x11578);
    Test::TestPacked(*decoder, etalon, tests);
  }

  void TestSolid(const Dump& etalon)
  {
    std::cout << "Testing solid archive" << std::endl;
    //Offsets:
    //0x14
    //0x2033
    Dump rar;
    Test::OpenFile("test_v2solid5.rar", rar);

    const Formats::Packed::Decoder::Ptr decoder = Formats::Packed::CreateRarDecoder();
    std::map<std::string, Dump> tests;
    const uint8_t* const data = &rar[0];
    tests["-p5"] = Dump(data + 0x14, data + 0x2033);
    tests["-p5solid"] = Dump(data + 0x2033, data + 0x2091);
    Test::TestPacked(*decoder, etalon, tests, false);
  }
}

int main()
{
  Dump etalon;
  Test::OpenFile("etalon.bin", etalon);

  try
  {
    TestBase(etalon);
    TestSolid(etalon);
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }
}
