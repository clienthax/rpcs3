﻿#include "utils.h"
#include "aes.h"
#include "key_vault.h"

SELF_KEY::SELF_KEY(u64 ver, u16 rev, u32 type, const std::string& e, const std::string& r, const std::string& pb, const std::string& pr, u32 ct)
{
	version = ver;
	revision = rev;
	self_type = type;
	hex_to_bytes(erk, e.c_str(), 0);
	hex_to_bytes(riv, r.c_str(), 0);
	hex_to_bytes(pub, pb.c_str(), 0);
	hex_to_bytes(priv, pr.c_str(), 0);
	curve_type = ct;
}

KeyVault::KeyVault()
{
}

void KeyVault::LoadSelfLV0Keys()
{
	sk_LV0_arr.clear();
	sk_LV0_arr.emplace_back(0x0000000000000000, 0x0000, KEY_LV0,
		"CA7A24EC38BDB45B98CCD7D363EA2AF0C326E65081E0630CB9AB2D215865878A",
		"F9205F46F6021697E670F13DFA726212",
		"A8FD6DB24532D094EFA08CB41C9A72287D905C6B27B42BE4AB925AAF4AFFF34D41EEB54DD128700D",
		"001AD976FCDE86F5B8FF3E63EF3A7F94E861975BA3",
		0x33);
}

void KeyVault::LoadSelfLDRKeys()
{
	sk_LDR_arr.clear();
	sk_LDR_arr.emplace_back(0x0000000000000000, 0x0000, KEY_LDR,
		"C0CEFE84C227F75BD07A7EB846509F93B238E770DACB9FF4A388F812482BE21B",
		"47EE7454E4774CC9B8960C7B59F4C14D",
		"C2D4AAF319355019AF99D44E2B58CA29252C89123D11D6218F40B138CAB29B7101F3AEB72A975019",
		"00C5B2BFA1A413DD16F26D31C0F2ED4720DCFB0670",
		0x20);
}

void KeyVault::LoadSelfLV1Keys()
{
	sk_LV1_arr.clear();
	sk_LV1_arr.emplace_back(0x0003003000000000, 0x0000, KEY_LV1,
		"B9F3F9E6107CFF2680A91E118C2403CF4A6F18F3C7EFD7D13D1AC4DB760BD222",
		"B43661B9A79BAD9D8E2B046469CDA1E7",
		"4C870BE86DDD996A92A3F7F404F33604244A1D02AB5B78BC9DAF030B78BE8867CF586171B7D45D20",
		"002CC736C7AD06D264E9AB663EB1F35F5DC159248C",
		0x33);
	sk_LV1_arr.emplace_back(0x0003004200000000, 0x0000, KEY_LV1,
		"B880593856C8C6D2037585626A12977F50DCFCF3F132D2C89AA6E670EAFC1646",
		"A79B05D4E37B8117A95E6E7C14FB640E",
		"7454C7CCBFC2F66C142D78A730A3A6F973CC0FB75A46FCBB390790138910A0CAC78E5E21F4DA3375",
		"00033A699FDD2DA6CDD6CCC03B2C6145F998706F74",
		0x34);
	sk_LV1_arr.emplace_back(0x0003005000000000, 0x0000, KEY_LV1,
		"1E8EEEA9E80A729F3FA52CF523B25941EA44B4155D94E5DADC5C5A77847620C7",
		"E034D31A80316960024D1B3D3164FDC3",
		"7E3A196f4A5879F3A7B091A2263F7C24E1716129B580566D308D9C2254B36AEE53DEF30EC85F8398",
		"005815D17125D04C33790321DE29EB6241365100B5",
		0x35);
	sk_LV1_arr.emplace_back(0x0003005500000000, 0x0000, KEY_LV1,
		"53ABDF84BE08B0351B734F2B97D2BE1621BC6C889E4362E5C70F39D6C3ED9F23",
		"44E652661AC7584DBE08ECB810FB5FC0",
		"733198A7759BC07326755BC9773A8A17C8A7043C7BDAB83D88E230512E2EA3852D7DA4263A7E97F9",
		"004312C65347ACBE95CC306442FEFD0AF4C2935EB3",
		0x05);
	sk_LV1_arr.emplace_back(0x0003005600000000, 0x0000, KEY_LV1,
		"48793EBDDA1AF65D737DA2FDA2DD104447A698F8A82CAAEE992831711BA94E83",
		"15DCF3C67147A45D09DE7521EECA07A1",
		"85A8868C320127F10B6598964C69221C086702021D31803520E21FDE4DBE827766BE41825CB7328C",
		"",
		0x07);
	sk_LV1_arr.emplace_back(0x0003006000000000, 0x0000, KEY_LV1,
		"5FF17D836E2C4AD69476E2614F64BDD05B9115389A9A6D055B5B544B1C34E3D5",
		"DF0F50EC3C4743C5B17839D7B49F24A4",
	    "1CDABE30833823F461CA534104115FFF60010B710631E435A7D915E82AE88EDE667264656CB7062E",
		"",
		0x05);
	sk_LV1_arr.emplace_back(0x0003006100000000, 0x0000, KEY_LV1,
		"5FF17D836E2C4AD69476E2614F64BDD05B9115389A9A6D055B5B544B1C34E3D5",
		"DF0F50EC3C4743C5B17839D7B49F24A4",
		"1CDABE30833823F461CA534104115FFF60010B710631E435A7D915E82AE88EDE667264656CB7062E",
		"",
		0x05);
	sk_LV1_arr.emplace_back(0x0003006600000000, 0x0000, KEY_LV1,
		"BD0621FA19383C3C72ECBC3B008F1CD55FFD7C3BB7510BF11AD0CF0FC2B70951",
		"569AF3745E1E02E3E288273CDE244CD8",
		"21E26F11C2D69478609DD1BD278CDFC940D90386455BA52FCD1FA7E27AC2AFA826C79A10193B625C",
		"",
		0x07);
	sk_LV1_arr.emplace_back(0x0003007400000000, 0x0000, KEY_LV1,
		"41A6E0039041E9D8AAF4EF2F2A2971248EDBD96A3985611ED7B4CE73EE4804FE",
		"C8C98D5A5CE23AF5607A352AECACB0DC",
		"4389664390265F96C1A882374C0F856364E33DB09BE124A4666F9A12F0DD9C811EDD55BA21ED0667",
		"",
		0x12);
	sk_LV1_arr.emplace_back(0x0004001100000000, 0x0000, KEY_LV1,
		"557EDF6C063F3272B0D44EEC12F418DA774815B5415597CC5F75C21E048BAD74",
		"7144D7574937818517826227EF4AC0B4",
		"085D38DBF9B757329EB862107929909D32FA1DAE60641BF4AC25319D7650597EE977F8E810FEEA96",
		"",
		0x13);
	sk_LV1_arr.emplace_back(0x0004005000000000, 0x0000, KEY_LV1,
		"10CEA04973FCCC12EC19924510822D8D4C41F657FD3D7E73F415A8D687421BCD",
		"ED8699562C6AC65204FA166257E7FCF4",
		"9AF86FC869C159FBB62F7D9674EE257ABF12E5A96D5875B4AA73C13C2BC13E2A4079F98B9B935EE2",
		"",
		0x14);
}

void KeyVault::LoadSelfLV2Keys()
{
	sk_LV2_arr.clear();
	sk_LV2_arr.emplace_back(0x0003003000000000, 0x0000, KEY_LV2,
		"94303F69513572AB5AE17C8C2A1839D2C24C28F65389D3BBB11894CE23E0798F",
		"9769BFD187B90990AE5FEA4E110B9CF5",
		"AFAF5E96AF396CBB69071082C46A8F34A030E8EDB799E0A7BE00AA264DFF3AEBF7923920D559404D",
		"0070ABF9361B02291829D479F56AB248203CD3EB46",
		0x20);
	sk_LV2_arr.emplace_back(0x0003004200000000, 0x0000, KEY_LV2,
		"575B0A6C4B4F2760A03FE4189EBAF4D947279FD982B14070349098B08FF92C10",
		"411CB18F460CE50CAF2C426D8F0D93C8",
		"3FEE313954CB3039C321A7E33B97FFDEC8988A8B55759161B04DBF4731284E4A8191E3F17D32B0EA",
		"0073076441A08CD179E5FACE349B86DA58B5B7BA78",
		0x21);
	sk_LV2_arr.emplace_back(0x0003005000000000, 0x0000, KEY_LV2,
		"6DBD48D787C58803A8D724DA5ACF04FF8FCE91D7545D2322F2B7ABF57014AF68",
		"603A36213708520ED5D745DEC1325BA5",
		"5888CB83AC3CCA9610BC173C53141C0CA58B93719E744660CA8823D5EAEE8F9BF736997054E4B7E3",
		"0009EBC3DE442FA5FBF6C4F3D4F9EAB07778A142BD",
		0x22);
	sk_LV2_arr.emplace_back(0x0003005500000000, 0x0000, KEY_LV2,
		"84015E90FA23139628A3C75CC09714E6427B527A82D18ABC3E91CD8D7DDAFF17",
		"5B240444D645F2038118F97FD5A145D5",
		"B266318245266B2D33641CD8A864066D077FAC60B7E27399099A70A683454B70F9888E7CC0C2BF72",
		"009D4CBA2BFB1A8330D3E20E59D281D476D231C73A",
		0x32);
	sk_LV2_arr.emplace_back(0x0003005600000000, 0x0000, KEY_LV2,
		"EAE15444048EFDE7A831BFA9F5D96F047C9FCFF50723E292CF50F5417D81E359",
		"9CA9282DC7FA9F315EF3156D970B7CD4",
		"0D58938CB47598A6A672874F1768068F8B80D8D17014D2ABEBAC85E5B0993D9FB6F307DDC3DDA699",
		"",
		0x33);
	sk_LV2_arr.emplace_back(0x0003006000000000, 0x0000, KEY_LV2,
		"88AD367EDEC2FEED3E2F99B1C685075C41BDEC90C84F526CAF588F89BBD1CBCC",
		"8D18E8E525230E63DE10291C9DD615BF",
	    "86EED1D65E58890ABDA9ACA486A2BDDB9C0A529C2053FAE301F0F698EAF443DA0F60595A597A7027",
		"",
		0x32);
	sk_LV2_arr.emplace_back(0x0003006100000000, 0x0000, KEY_LV2,
		"88AD367EDEC2FEED3E2F99B1C685075C41BDEC90C84F526CAF588F89BBD1CBCC",
		"8D18E8E525230E63DE10291C9DD615BF",
		"86EED1D65E58890ABDA9ACA486A2BDDB9C0A529C2053FAE301F0F698EAF443DA0F60595A597A7027",
		"",
		0x32);
	sk_LV2_arr.emplace_back(0x0003006600000000, 0x0000, KEY_LV2,
		"688D5FCAC6F4EA35AC6AC79B10506007286131EE038116DB8AA2C0B0340D9FB0",
		"75E0239D18B0B669EAE650972F99726B",
		"008E1C820AC567D1BFB8FE3CC6AD2E1845A1D1B19ED2E18B18CA34A8D28A83EC60C63859CDB3DACA",
		"",
		0x33);
	sk_LV2_arr.emplace_back(0x0003007400000000, 0x0000, KEY_LV2,
		"E81C5B04C29FB079A4A2687A39D4EA97BFB49D80EF546CEB292979A5F77A6254",
		"15058FA7F2CAD7C528B5F605F6444EB0",
		"438D0E5C1E7AFB18234DB6867472FF5F52B750F30C379C7DD1EE0FD23E417B3EA819CC01BAC480ED",
		"",
		0x11);
	sk_LV2_arr.emplace_back(0x0004001100000000, 0x0000, KEY_LV2,
		"A1E4B86ED02BF7F1372A2C73FE02BC738907EB37CE3BA605FE783C999FAFDB97",
		"BBE7799B9A37CB272E386618FDFD4AEC",
		"5B31A8E2A663EBD673196E2E1022E0D64988C4E1BBFE5E474415883A3BA0D9C562A2BE9C30E9B4A8",
		"",
		0x07);
	sk_LV2_arr.emplace_back(0x0004005000000000, 0x0000, KEY_LV2,
		"0CAF212B6FA53C0DA7E2C575ADF61DBE68F34A33433B1B891ABF5C4251406A03",
		"9B79374722AD888EB6A35A2DF25A8B3E",
		"1034A6F98AF6625CC3E3604B59B971CA617DF337538D2179EBB22F3BDC9D0C6DA56BA7DDFD205A50",
		"",
		0x14);
}

void KeyVault::LoadSelfISOKeys()
{
	sk_ISO_arr.clear();
	sk_ISO_arr.emplace_back(0x0003003000000000, 0x0001, KEY_ISO,
		"8860D0CFF4D0DC688D3223321B96B59A777E6914961488E07048DAECB020ECA4",
		"C82D015D46CF152F1DD0C16F18B5B1E5",
		"733918D7C888130509346E6B4A8B6CAA357AB557E814E8122BF102C14A314BF9475B9D70EAF9EC29",
		"009BE892E122A5C943C1BB7403A67318AA9E1B286F",
		0x36);
	sk_ISO_arr.emplace_back(0x0003004200000000, 0x0001, KEY_ISO,
		"101E27F3FA2FB53ACA924F783AD553162D56B975D05B81351A1111799F20254D",
		"8D2E9C6297B8AD252998458296AC773C",
		"138446EE0BDDA5638F97328C8956E6489CBBFE57C5961D40DD5C43BB4138F1C400A8B27204A5D625",
		"00849DBC57D3B92F01864E6E82EB4EF0EF6311E122",
		0x32);
	sk_ISO_arr.emplace_back(0x0003005000000000, 0x0001, KEY_ISO,
		"3F2604FA27AEADFBE1AC69EB00BB16EF196C2193CBD62900FFD8C25041680843",
		"A414AC1DB7987E43777651B330B899E1",
		"1F4633AFDE18614D6CEF38A2FD6C4CCAC7B6EB8109D72CD066ECEBA0193EA3F43C37AE83179A4E5F",
		"0085B4B05DEBA7E6AD831653C974D95149803BB272",
		0x33);
	sk_ISO_arr.emplace_back(0x0003005500000000, 0x0001, KEY_ISO,
		"BDB74AA6E3BA2DC10B1BD7F17198399A158DBE1FA0BEA68C90FCACBE4D04BE37",
		"0207A479B1574F8E7F697528F05D5435",
		"917E1F1DC48A54EB5F10B38E7569BB5383628A7C906F0DCA62FDA33805C15FAB270016940A09DB58",
		"00294411363290975BA551336D3965D88AF029A17B",
		0x03);
	sk_ISO_arr.emplace_back(0x0003005600000000, 0x0001, KEY_ISO,
		"311C015F169F2A1E0757F7064B14C7C9F3A3FFEE015BD4E3A22401A2667857CE",
		"7BB8B3F5AC8E0890E3148AE5688C7350",
		"3F040EFA2335FED5670BA4D5C3AB2D9D0B4BA69D154A0062EA995A7D21DBAF0DC5A0DAD333D1C1DD",
		"",
		0x08);
	sk_ISO_arr.emplace_back(0x0003006000000000, 0x0001, KEY_ISO,
		"8474ADCA3B3244931EECEB9357841442442A1C4A4BCF4E498E6738950F4E4093",
		"FFF9CACCC4129125CAFB240F419E5F39",
	    "098E1A53E59A95316B00D5A29C05FFEBAE41D1A8A386F9DA96F98858FD25E07BB7A3BC96A5D5B556",
		"",
		0x03);
	sk_ISO_arr.emplace_back(0x0003006100000000, 0x0001, KEY_ISO,
		"8474ADCA3B3244931EECEB9357841442442A1C4A4BCF4E498E6738950F4E4093",
		"FFF9CACCC4129125CAFB240F419E5F39",
		"098E1A53E59A95316B00D5A29C05FFEBAE41D1A8A386F9DA96F98858FD25E07BB7A3BC96A5D5B556",
		"",
		0x03);
	sk_ISO_arr.emplace_back(0x0003006600000000, 0x0001, KEY_ISO,
		"E6A21C599B75696C169EC02582BDA74A776134A6E05108EA701EC0CA2AC03592",
		"D292A7BD57C0BB2EABBCA1252FA9EDEF",
		"2ED078A13DC4617EB550AD06E228C83C142A2D588EB5E729402D18038A14842FD65B277DCAD225A5",
		"",
		0x08);
	sk_ISO_arr.emplace_back(0x0003007400000000, 0x0001, KEY_ISO,
		"072D3A5C3BDB0D674DE209381432B20414BC9BDA0F583ECB94BD9A134176DD51",
		"8516A81F02CF938740498A406C880871",
		"5A778DEB5C4F12E8D48E06A2BBBBE3C90FA8C6C47DF9BDB5697FD4A8EB7941CE3F59A557E81C787D",
		"",
		0x21);
	sk_ISO_arr.emplace_back(0x0003007400000000, 0x0100, KEY_ISO,
		"786FAB8A0B89474A2CB80B3EA104CCCB9E13F66B45EC499BB31865D07C661EA8",
		"94662F13D99A9F5D211C979FFDF65FE3",
		"912C94C252B7799CEB45DFBB73EF7CAD9BCC0793A3331BBB79E3C47C0F5C782F698065A8D4DB0D8B",
		"",
		0x0E);
	sk_ISO_arr.emplace_back(0x0004001100000000, 0x0001, KEY_ISO,
		"4262657A3185D9480F82C8BD2F81766FCC2C8FD7DD5EBE8657B00B939E0C75BD",
		"4F1E3EF07D893A4714B1B3D5A4E50479",
		"4DBFCFA68B52F1D66E09AFA6C18EC65479EDBD027B6B8C6A5D85FE5C84D43EA40CEF1672078A0702",
		"",
		0x11);
	sk_ISO_arr.emplace_back(0x0004001100000000, 0x0100, KEY_ISO,
		"16AA7D7C35399E2B1BFAF68CD19D7512A7855029C08BECC4CC3F035DF7F9C70B",
		"0E50DB6D937D262CB0499136852FCB80",
		"AEE2795BF295662A50DFAFE70D1B0B6F0A2EBB211E1323A275FC6E2D13BE4F2F10CA34784F4CF1EC",
		"",
		0x0F);
	sk_ISO_arr.emplace_back(0x0004005000000000, 0x0001, KEY_ISO,
		"63565DBE98C3B1A52AADC907C47130FE57A10734E84F22592670F86ED2B0A086",
		"953F6A99891B4739358F5363A00C08B9",
		"26BE7B02E7D65C6C21BF4063CDB8C0092FE1679D62FA1A8CCC284A1D21885473A959992537A06612",
		"",
		0x15);
	sk_ISO_arr.emplace_back(0x0004005000000000, 0x0100, KEY_ISO,
		"B96EA32CB96EA32DB96EA32CB96EA32CB96EA32CB96EA32DB96EA32CB96EA32C",
		"B96EA32CB96EA32DB96EA32DB96EA32C",
		"2D7066E68C6AC3373B1346FD76FE7D18A207C811500E65D85DB57BC4A27AD78F59FD53F38F50E151",
		"0044AA25B4276D79B494A29CB8DE104642424F8EEF",
		0x02);
}

void KeyVault::LoadSelfAPPKeys()
{
	sk_APP_arr.clear();
	sk_APP_arr.emplace_back(0x0000009200000000, 0x0000, KEY_APP,
		"95F50019E7A68E341FA72EFDF4D60ED376E25CF46BB48DFDD1F080259DC93F04",
		"4A0955D946DB70D691A640BB7FAECC4C",
		"6F8DF8EBD0A1D1DB08B30DD3A951E3F1F27E34030B42C729C55555232D61B834B8BDFFB07E54B343",
		"006C3E4CCB2C69A5AD7C6F60448E50C7F9184EEAF4",
		0x21);
	sk_APP_arr.emplace_back(0x0003003000000000, 0x0001, KEY_APP,
		"79481839C406A632BDB4AC093D73D99AE1587F24CE7E69192C1CD0010274A8AB",
		"6F0F25E1C8C4B7AE70DF968B04521DDA",
		"94D1B7378BAFF5DFED269240A7A364ED68446741622E50BC6079B6E606A2F8E0A4C56E5CFF836526",
		"003DE80167D2F0E9D30F2145144A558D1174F5410C",
		0x11);
	sk_APP_arr.emplace_back(0x0003003000000000, 0x0002, KEY_APP,
		"4F89BE98DDD43CAD343F5BA6B1A133B0A971566F770484AAC20B5DD1DC9FA06A",
		"90C127A9B43BA9D8E89FE6529E25206F",
		"8CA6905F46148D7D8D84D2AFCEAE61B41E6750FC22EA435DFA61FCE6F4F860EE4F54D9196CA5290E",
		"",
		0x13);
	sk_APP_arr.emplace_back(0x0003003000000000, 0x0003, KEY_APP,
		"C1E6A351FCED6A0636BFCB6801A0942DB7C28BDFC5E0A053A3F52F52FCE9754E",
		"E0908163F457576440466ACAA443AE7C",
		"50022D5D37C97905F898E78E7AA14A0B5CAAD5CE8190AE5629A10D6F0CF4173597B37A95A7545C92",
		"",
		0x0B);
	sk_APP_arr.emplace_back(0x0003004200000000, 0x0004, KEY_APP,
		"838F5860CF97CDAD75B399CA44F4C214CDF951AC795298D71DF3C3B7E93AAEDA",
		"7FDBB2E924D182BB0D69844ADC4ECA5B",
		"1F140E8EF887DAB52F079A06E6915A6460B75CD256834A43FA7AF90C23067AF412EDAFE2C1778D69",
		"0074E922FDEE5DC4CDF22FC8D7986477F813400860",
		0x14);
	sk_APP_arr.emplace_back(0x0003004200000000, 0x0005, KEY_APP,
		"C109AB56593DE5BE8BA190578E7D8109346E86A11088B42C727E2B793FD64BDC",
		"15D3F191295C94B09B71EBDE088A187A",
		"B6BB0A84C649A90D97EBA55B555366F52381BB38A84C8BB71DA5A5A0949043C6DB249029A43156F7",
		"",
		0x15);
	sk_APP_arr.emplace_back(0x0003004200000000, 0x0006, KEY_APP,
		"6DFD7AFB470D2B2C955AB22264B1FF3C67F180983B26C01615DE9F2ECCBE7F41",
		"24BD1C19D2A8286B8ACE39E4A37801C2",
		"71F46AC33FF89DF589A100A7FB64CEAC244C9A0CBBC1FDCE80FB4BF8A0D2E66293309CB8EE8CFA95",
		"",
		0x2C);
	sk_APP_arr.emplace_back(0x0003005000000000, 0x0007, KEY_APP,
		"945B99C0E69CAF0558C588B95FF41B232660ECB017741F3218C12F9DFDEEDE55",
		"1D5EFBE7C5D34AD60F9FBC46A5977FCE",
		"AB284CA549B2DE9AA5C903B75652F78D192F8F4A8F3CD99209415C0A84C5C9FD6BF3095C1C18FFCD",
		"002CF896D35DB871D0E6A252E799876A70D043C23E",
		0x15);
	sk_APP_arr.emplace_back(0x0003005000000000, 0x0008, KEY_APP,
		"2C9E8969EC44DFB6A8771DC7F7FDFBCCAF329EC3EC070900CABB23742A9A6E13",
		"5A4CEFD5A9C3C093D0B9352376D19405",
		"6E82F6B54A0E9DEBE4A8B3043EE3B24CD9BBB62B4416B0482582E419A2552E29AB4BEA0A4D7FA2D5",
		"",
		0x16);
	sk_APP_arr.emplace_back(0x0003005000000000, 0x0009, KEY_APP,
		"F69E4A2934F114D89F386CE766388366CDD210F1D8913E3B973257F1201D632B",
		"F4D535069301EE888CC2A852DB654461",
		"1D7B974D10E61C2ED087A0981535904677EC07E96260F89565FF7EBDA4EE035C2AA9BCBDD5893F99",
		"",
		0x2D);
	sk_APP_arr.emplace_back(0x0003005500000000, 0x000A, KEY_APP,
		"29805302E7C92F204009161CA93F776A072141A8C46A108E571C46D473A176A3",
		"5D1FAB844107676ABCDFC25EAEBCB633",
		"09301B6436C85B53CB1585300A3F1AF9FB14DB7C30088C4642AD66D5C148B8995BB1A698A8C71827",
		"0010818ED8A666051C6198662C3D6DDE2CA4901DDC",
		0x25);
	sk_APP_arr.emplace_back(0x0003005500000000, 0x000B, KEY_APP,
		"A4C97402CC8A71BC7748661FE9CE7DF44DCE95D0D58938A59F47B9E9DBA7BFC3",
		"E4792F2B9DB30CB8D1596077A13FB3B5",
		"2733C889D289550FE00EAA5A47A34CEF0C1AF187610EB07BA35D2C09BB73C80B244EB4147700D1BF",
		"",
		0x26);
	sk_APP_arr.emplace_back(0x0003005500000000, 0x000C, KEY_APP,
		"9814EFFF67B7074D1B263BF85BDC8576CE9DEC914123971B169472A1BC2387FA",
		"D43B1FA8BE15714B3078C23908BB2BCA",
		"7D1986C6BEE6CE1E0C5893BD2DF203881F40D5056761CC3F1F2E9D9A378617A2DE40BA5F09844CEB",
		"",
		0x3D);
	sk_APP_arr.emplace_back(0x0003005600000000, 0x000D, KEY_APP,
		"03B4C421E0C0DE708C0F0B71C24E3EE04306AE7383D8C5621394CCB99FF7A194",
		"5ADB9EAFE897B54CB1060D6885BE22CF",
		"71502ADB5783583AB88B2D5F23F419AF01C8B1E72FCA1E694AD49FE3266F1F9C61EFC6F29B351142",
		"",
		0x12);
	sk_APP_arr.emplace_back(0x0003005600000000, 0x000E, KEY_APP,
		"39A870173C226EB8A3EEE9CA6FB675E82039B2D0CCB22653BFCE4DB013BAEA03",
		"90266C98CBAA06C1BF145FF760EA1B45",
		"84DE5692809848E5ACBE25BE548F6981E3DB14735A5DDE1A0FD1F475866532B862B1AB6A004B7255",
		"",
		0x27);
	sk_APP_arr.emplace_back(0x0003005600000000, 0x000F, KEY_APP,
		"FD52DFA7C6EEF5679628D12E267AA863B9365E6DB95470949CFD235B3FCA0F3B",
		"64F50296CF8CF49CD7C643572887DA0B",
		"0696D6CCBD7CF585EF5E00D547503C185D7421581BAD196E081723CD0A97FA40B2C0CD2492B0B5A1",
		"",
		0x3A);
	sk_APP_arr.emplace_back(0x0003006100000000, 0x0010, KEY_APP,
		"A5E51AD8F32FFBDE808972ACEE46397F2D3FE6BC823C8218EF875EE3A9B0584F",
		"7A203D5112F799979DF0E1B8B5B52AA4",
		"50597B7F680DD89F6594D9BDC0CBEE03666AB53647D0487F7F452FE2DD02694631EA755548C9E934",
		"",
		0x25);
	sk_APP_arr.emplace_back(0x0003006100000000, 0x0011, KEY_APP,
		"0F8EAB8884A51D092D7250597388E3B8B75444AC138B9D36E5C7C5B8C3DF18FD",
		"97AF39C383E7EF1C98FA447C597EA8FE",
		"2FDA7A56AAEA65921C0284FF1942C6DE137370093D106034B59191951A5201B422D462F8726F852D",
		"",
		0x26);
	sk_APP_arr.emplace_back(0x0003006600000000, 0x0013, KEY_APP,
		"DBF62D76FC81C8AC92372A9D631DDC9219F152C59C4B20BFF8F96B64AB065E94",
		"CB5DD4BE8CF115FFB25801BC6086E729",
		"B26FE6D3E3A1E766FAE79A8E6A7F48998E7FC1E4B0AD8745FF54C018C2A6CC7A0DD7525FAFEA4917",
		"",
		0x12);
	sk_APP_arr.emplace_back(0x0003006600000000, 0x0014, KEY_APP,
		"491B0D72BB21ED115950379F4564CE784A4BFAABB00E8CB71294B192B7B9F88E",
		"F98843588FED8B0E62D7DDCB6F0CECF4",
		"04275E8838EF95BD013B223C3DF674540932F21B534C7ED2944B9104D938FEB03B824DDB866AB26E",
		"",
		0x27);
	sk_APP_arr.emplace_back(0x0003007400000000, 0x0016, KEY_APP,
		"A106692224F1E91E1C4EBAD4A25FBFF66B4B13E88D878E8CD072F23CD1C5BF7C",
		"62773C70BD749269C0AFD1F12E73909E",
		"566635D3E1DCEC47243AAD1628AE6B2CEB33463FC155E4635846CE33899C5E353DDFA47FEF5694AF",
		"",
		0x30);
	sk_APP_arr.emplace_back(0x0003007400000000, 0x0017, KEY_APP,
		"4E104DCE09BA878C75DA98D0B1636F0E5F058328D81419E2A3D22AB0256FDF46",
		"954A86C4629E116532304A740862EF85",
		"3B7B04C71CAE2B1199D57453C038BB1B541A05AD1B94167B0AB47A9B24CAECB9000CB21407009666",
		"",
		0x08);
	sk_APP_arr.emplace_back(0x0003007400000000, 0x0018, KEY_APP,
		"1F876AB252DDBCB70E74DC4A20CD8ED51E330E62490E652F862877E8D8D0F997",
		"BF8D6B1887FA88E6D85C2EDB2FBEC147",
		"64A04126D77BF6B4D686F6E8F87DD150A5B014BA922D2B694FFF4453E11239A6E0B58F1703C51494",
		"",
		0x11);
	sk_APP_arr.emplace_back(0x0004001100000000, 0x0019, KEY_APP,
		"3236B9937174DF1DC12EC2DD8A318A0EA4D3ECDEA5DFB4AC1B8278447000C297",
		"6153DEE781B8ADDC6A439498B816DC46",
		"148DCA961E2738BAF84B2D1B6E2DA2ABD6A95F2C9571E54C6922F9ED9674F062B7F1BE5BD6FA5268",
		"",
		0x31);
	sk_APP_arr.emplace_back(0x0004001100000000, 0x001A, KEY_APP,
		"5EFD1E9961462794E3B9EF2A4D0C1F46F642AAE053B5025504130590E66F19C9",
		"1AC8FA3B3C90F8FDE639515F91B58327",
		"BE4B1B513536960618BFEF12A713F6673881B02F9DC616191E823FC8337CCF99ADAA6172019C0C23",
		"",
		0x17);
	sk_APP_arr.emplace_back(0x0004001100000000, 0x001B, KEY_APP,
		"66637570D1DEC098467DB207BAEA786861964D0964D4DBAF89E76F46955D181B",
		"9F7B5713A5ED59F6B35CD8F8A165D4B8",
		"4AB6FB1F6F0C3D9219923C1AC683137AB05DF667833CC6A5E8F590E4E28FE2EB180C7D5861117CFB",
		"",
		0x12);
	sk_APP_arr.emplace_back(0x0004004600000000, 0x001C, KEY_APP,
		"CFF025375BA0079226BE01F4A31F346D79F62CFB643CA910E16CF60BD9092752",
		"FD40664E2EBBA01BF359B0DCDF543DA4",
		"36C1ACE6DD5CCC0006FDF3424750FAC515FC5CFA2C93EC53C6EC2BC421708D154E91F2E7EA54A893",
		"",
		0x09);
	sk_APP_arr.emplace_back(0x0004004600000000, 0x001D, KEY_APP,
		"D202174EB65A62048F3674B59EF6FE72E1872962F3E1CD658DE8D7AF71DA1F3E",
		"ACB9945914EBB7B9A31ECE320AE09F2D",
		"430322887503CF52928FAAA410FD623C7321281C8825D95F5B47EF078EFCFC44454C3AB4F00BB879",
		"",
		0x1A);
}

void KeyVault::LoadSelfNPDRMKeys()
{
	sk_NPDRM_arr.clear();
	sk_NPDRM_arr.emplace_back(0x0001000000000000, 0x0001, KEY_NPDRM,
		"F9EDD0301F770FABBA8863D9897F0FEA6551B09431F61312654E28F43533EA6B",
		"A551CCB4A42C37A734A2B4F9657D5540",
		"B05F9DA5F9121EE4031467E74C505C29A8E29D1022379EDFF0500B9AE480B5DAB4578A4C61C5D6BF",
		"00040AB47509BED04BD96521AD1B365B86BF620A98",
		0x11);
	sk_NPDRM_arr.emplace_back(0x0001000000000000, 0x0002, KEY_NPDRM,
		"8E737230C80E66AD0162EDDD32F1F774EE5E4E187449F19079437A508FCF9C86",
		"7AAECC60AD12AED90C348D8C11D2BED5",
		"05BF09CB6FD78050C78DE69CC316FF27C9F1ED66A45BFCE0A1E5A6749B19BD546BBB4602CF373440",
		"",
		0x0A);
	sk_NPDRM_arr.emplace_back(0x0003003000000000, 0x0003, KEY_NPDRM,
		"1B715B0C3E8DC4C1A5772EBA9C5D34F7CCFE5B82025D453F3167566497239664",
		"E31E206FBB8AEA27FAB0D9A2FFB6B62F",
		"3F51E59FC74D6618D34431FA67987FA11ABBFACC7111811473CD9988FE91C43FC74605E7B8CB732D",
		"",
		0x08);
	sk_NPDRM_arr.emplace_back(0x0003004200000000, 0x0004, KEY_NPDRM,
		"BB4DBF66B744A33934172D9F8379A7A5EA74CB0F559BB95D0E7AECE91702B706",
		"ADF7B207A15AC601110E61DDFC210AF6",
		"9C327471BAFF1F877AE4FE29F4501AF5AD6A2C459F8622697F583EFCA2CA30ABB5CD45D1131CAB30",
		"00B61A91DF4AB6A9F142C326BA9592B5265DA88856",
		0x16);
	sk_NPDRM_arr.emplace_back(0x0003004200000000, 0x0006, KEY_NPDRM,
		"8B4C52849765D2B5FA3D5628AFB17644D52B9FFEE235B4C0DB72A62867EAA020",
		"05719DF1B1D0306C03910ADDCE4AF887",
		"2A5D6C6908CA98FC4740D834C6400E6D6AD74CF0A712CF1E7DAE806E98605CC308F6A03658F2970E",
		"",
		0x29);
	sk_NPDRM_arr.emplace_back(0x0003005000000000, 0x0007, KEY_NPDRM,
		"3946DFAA141718C7BE339A0D6C26301C76B568AEBC5CD52652F2E2E0297437C3",
		"E4897BE553AE025CDCBF2B15D1C9234E",
		"A13AFE8B63F897DA2D3DC3987B39389DC10BAD99DFB703838C4A0BC4E8BB44659C726CFD0CE60D0E",
		"009EF86907782A318D4CC3617EBACE2480E73A46F6",
		0x17);
	sk_NPDRM_arr.emplace_back(0x0003005000000000, 0x0009, KEY_NPDRM,
		"0786F4B0CA5937F515BDCE188F569B2EF3109A4DA0780A7AA07BD89C3350810A",
		"04AD3C2F122A3B35E804850CAD142C6D",
		"A1FE61035DBBEA5A94D120D03C000D3B2F084B9F4AFA99A2D4A588DF92B8F36327CE9E47889A45D0",
		"",
		0x2A);
	sk_NPDRM_arr.emplace_back(0x0003005500000000, 0x000A, KEY_NPDRM,
		"03C21AD78FBB6A3D425E9AAB1298F9FD70E29FD4E6E3A3C151205DA50C413DE4",
		"0A99D4D4F8301A88052D714AD2FB565E",
		"3995C390C9F7FBBAB124A1C14E70F9741A5E6BDF17A605D88239652C8EA7D5FC9F24B30546C1E44B",
		"009AC6B22A056BA9E0B6D1520F28A57A3135483F9F",
		0x27);
	sk_NPDRM_arr.emplace_back(0x0003005500000000, 0x000C, KEY_NPDRM,
		"357EBBEA265FAEC271182D571C6CD2F62CFA04D325588F213DB6B2E0ED166D92",
		"D26E6DD2B74CD78E866E742E5571B84F",
		"00DCF5391618604AB42C8CFF3DC304DF45341EBA4551293E9E2B68FFE2DF527FFA3BE8329E015E57",
		"",
		0x3A);
	sk_NPDRM_arr.emplace_back(0x0003005600000000, 0x000D, KEY_NPDRM,
		"337A51416105B56E40D7CAF1B954CDAF4E7645F28379904F35F27E81CA7B6957",
		"8405C88E042280DBD794EC7E22B74002",
		"9BFF1CC7118D2393DE50D5CF44909860683411A532767BFDAC78622DB9E5456753FE422CBAFA1DA1",
		"",
		0x18);
	sk_NPDRM_arr.emplace_back(0x0003005600000000, 0x000F, KEY_NPDRM,
		"135C098CBE6A3E037EBE9F2BB9B30218DDE8D68217346F9AD33203352FBB3291",
		"4070C898C2EAAD1634A288AA547A35A8",
		"BBD7CCCB556C2EF0F908DC7810FAFC37F2E56B3DAA5F7FAF53A4944AA9B841F76AB091E16B231433",
		"",
		0x3B);
	sk_NPDRM_arr.emplace_back(0x0003006100000000, 0x0010, KEY_NPDRM,
		"4B3CD10F6A6AA7D99F9B3A660C35ADE08EF01C2C336B9E46D1BB5678B4261A61",
		"C0F2AB86E6E0457552DB50D7219371C5",
		"64A5C60BC2AD18B8A237E4AA690647E12BF7A081523FAD4F29BE89ACAC72F7AB43C74EC9AFFDA213",
		"",
		0x27);
	sk_NPDRM_arr.emplace_back(0x0003006600000000, 0x0013, KEY_NPDRM,
		"265C93CF48562EC5D18773BEB7689B8AD10C5EB6D21421455DEBC4FB128CBF46",
		"8DEA5FF959682A9B98B688CEA1EF4A1D",
		"9D8DB5A880608DC69717991AFC3AD5C0215A5EE413328C2ABC8F35589E04432373DB2E2339EEF7C8",
		"",
		0x18);
	sk_NPDRM_arr.emplace_back(0x0003007400000000, 0x0016, KEY_NPDRM,
		"7910340483E419E55F0D33E4EA5410EEEC3AF47814667ECA2AA9D75602B14D4B",
		"4AD981431B98DFD39B6388EDAD742A8E",
		"62DFE488E410B1B6B2F559E4CB932BCB78845AB623CC59FDF65168400FD76FA82ED1DC60E091D1D1",
		"",
		0x25);
	sk_NPDRM_arr.emplace_back(0x0004001100000000, 0x0019, KEY_NPDRM,
		"FBDA75963FE690CFF35B7AA7B408CF631744EDEF5F7931A04D58FD6A921FFDB3",
		"F72C1D80FFDA2E3BF085F4133E6D2805",
		"637EAD34E7B85C723C627E68ABDD0419914EBED4008311731DD87FDDA2DAF71F856A70E14DA17B42",
		"",
		0x24);
	sk_NPDRM_arr.emplace_back(0x0004004600000000, 0x001C, KEY_NPDRM,
		"8103EA9DB790578219C4CEDF0592B43064A7D98B601B6C7BC45108C4047AA80F",
		"246F4B8328BE6A2D394EDE20479247C5",
		"503172C9551308A87621ECEE90362D14889BFED2CF32B0B3E32A4F9FE527A41464B735E1ADBC6762",
		"",
		0x30);
}

void KeyVault::LoadSelfUNK7Keys()
{
	sk_UNK7_arr.clear();
	sk_UNK7_arr.emplace_back(0x0003003000000000, 0x0000, KEY_UNK7,
		"D91166973979EA8694476B011AC62C7E9F37DA26DE1E5C2EE3D66E42B8517085",
		"DC01280A6E46BC674B81A7E8801EBE6E",
		"A0FC44108236141BF3517A662B027AFC1AC513A05690496C754DEB7D43BDC41B80FD75C212624EE4",
		"",
		0x11);
	sk_UNK7_arr.emplace_back(0x0003004200000000, 0x0000, KEY_UNK7,
		"B73111B0B00117E48DE5E2EE5E534C0F0EFFA4890BBB8CAD01EE0F848F91583E",
		"86F56F9E5DE513894874B8BA253334B1",
		"B0BA1A1AB9723BB4E87CED9637BE056066BC56E16572D43D0210A06411DBF8FEB8885CD912384AE5",
		"",
		0x12);
	sk_UNK7_arr.emplace_back(0x0003005000000000, 0x0000, KEY_UNK7,
		"8E944267C02E69A4FE474B7F5FCD7974A4F936FF4355AEC4F80EFA123858D8F6",
		"908A75754E521EAC2F5A4889C6D7B72D",
		"91201DA7D79E8EE2563142ECBD646DA026C963AC09E760E5390FFE24DAE6864310ABE147F8204D0B",
		"",
		0x13);
	sk_UNK7_arr.emplace_back(0x0003005500000000, 0x0000, KEY_UNK7,
		"BB31DF9A6F62C0DF853075FAA65134D9CE2240306C1731D1F7DA9B5329BD699F",
		"263057225873F83940A65C8C926AC3E4",
		"BC3A82A4F44C43A197070CD236FDC94FCC542D69A3E803E0AFF78D1F3DA19A79D2F61FAB5B94B437",
		"",
		0x23);
	sk_UNK7_arr.emplace_back(0x0003005600000000, 0x0000, KEY_UNK7,
		"71AA75C70A255580E4AE9BDAA0B08828C53EAA713CD0713797F143B284C1589B",
		"9DED878CB6BA07121C0F50E7B172A8BF",
		"387FCDAEAFF1B59CFAF79CE6215A065ACEAFFAF4048A4F217E1FF5CE67C66EC3F089DB235E52F9D3",
		"",
		0x29);
	sk_UNK7_arr.emplace_back(0x0003006100000000, 0x0000, KEY_UNK7,
		"F5D1DBC182F5083CD4EA37C431C7DAC73882C07F232D2699B1DD9FDDF1BF4195",
		"D3A7C3C91CBA014FCBCA6D5570DE13FF",
		"97CA8A9781F45E557E98F176EF794FCDA6B151EB3DFD1ABA12151E00AE59957C3B15628FC8875D28",
		"",
		0x23);
	sk_UNK7_arr.emplace_back(0x0003006600000000, 0x0000, KEY_UNK7,
		"BF10F09590C0152F7EF749FF4B990122A4E8E5491DA49A2D931E72EEB990F860",
		"22C19C5522F7A782AFC547C2640F5BDE",
		"3233BA2B284189FB1687DF653002257A0925D8EB0C64EBBE8CC7DE87F548D107DE1FD3D1D285DB4F",
		"",
		0x29);
	sk_UNK7_arr.emplace_back(0x0003007400000000, 0x0000, KEY_UNK7,
		"F11DBD2C97B32AD37E55F8E743BC821D3E67630A6784D9A058DDD26313482F0F",
		"FC5FA12CA3D2D336C4B8B425D679DA55",
		"19E27EE90E33EDAB16B22E688B5F704E5C6EC1062070EBF43554CD03DFDAE16D684BB8B5574DBECA",
		"",
		0x15);
	sk_UNK7_arr.emplace_back(0x0004001100000000, 0x0000, KEY_UNK7,
		"751EE949CD3ADF50A469197494A1EC358409CCBE6E85217EBDE7A87D3FF1ABD8",
		"23AE4ADA4D3F798DC5ED98000337FF77",
		"1BABA87CD1AD705C462D4E7427B6DAF59A50383A348A15088F0EDFCF1ADF2B5C2B2D507B2A357D36",
		"",
		0x1A);
	sk_UNK7_arr.emplace_back(0x0004004600000000, 0x0000, KEY_UNK7,
		"46BD0891224E0CE13E2162921D4BB76193AEEE4416A729FCDD111C5536BF87C9",
		"BF036387CDB613C0AC88A6D9D2CC5316",
		"A14F6D5F9AD7EBB3B7A39A7C32F13E5DC3B0BA16BDC33D39FDDF88F4AEEA6CFEEB0C0796C917A952",
		"",
		0x0F);
}

SELF_KEY KeyVault::GetSelfLV0Key() const
{
	return sk_LV0_arr[0];
}

SELF_KEY KeyVault::GetSelfLDRKey() const
{
	return sk_LDR_arr[0];
}

SELF_KEY KeyVault::GetSelfLV1Key(u64 version) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_LV1_arr.size(); i++)
	{
		if (sk_LV1_arr[i].version == version)
		{
			key = sk_LV1_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::GetSelfLV2Key(u64 version) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_LV2_arr.size(); i++)
	{
		if (sk_LV2_arr[i].version == version)
		{
			key = sk_LV2_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::GetSelfISOKey(u16 revision, u64 version) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_ISO_arr.size(); i++)
	{
		if ((sk_ISO_arr[i].version == version) &&
			(sk_ISO_arr[i].revision == revision))
		{
			key = sk_ISO_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::GetSelfAPPKey(u16 revision) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_APP_arr.size(); i++)
	{
		if (sk_APP_arr[i].revision == revision)
		{
			key = sk_APP_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::GetSelfUNK7Key(u64 version) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_UNK7_arr.size(); i++)
	{
		if (sk_UNK7_arr[i].version == version)
		{
			key = sk_UNK7_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::GetSelfNPDRMKey(u16 revision) const
{
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	for(unsigned int i = 0; i < sk_NPDRM_arr.size(); i++)
	{
		if (sk_NPDRM_arr[i].revision == revision)
		{
			key = sk_NPDRM_arr[i];
			break;
		}
	}

	return key;
}

SELF_KEY KeyVault::FindSelfKey(u32 type, u16 revision, u64 version)
{
	// Empty key.
	SELF_KEY key(0, 0, 0, "", "", "", "", 0);

	key_vault_log.notice("FindSelfKey: Type:0x%x, Revision:0x%x, Version:0x%x", type, revision, version);


	// Check SELF type.
	switch (type)
	{
	case KEY_LV0:
		LoadSelfLV0Keys();
		key = GetSelfLV0Key();
		break;
	case KEY_LV1:
		LoadSelfLV1Keys();
		key = GetSelfLV1Key(version);
		break;
	case KEY_LV2:
		LoadSelfLV2Keys();
		key = GetSelfLV2Key(version);
		break;
	case KEY_APP:
		LoadSelfAPPKeys();
		key = GetSelfAPPKey(revision);
		break;
	case KEY_ISO:
		LoadSelfISOKeys();
		key = GetSelfISOKey(revision, version);
		break;
	case KEY_LDR:
		LoadSelfLDRKeys();
		key = GetSelfLDRKey();
		break;
	case KEY_UNK7:
		LoadSelfUNK7Keys();
		key = GetSelfUNK7Key(version);
		break;
	case KEY_NPDRM:
		LoadSelfNPDRMKeys();
		key = GetSelfNPDRMKey(revision);
		break;
	default:
		break;
	}

	return key;
}

void KeyVault::SetKlicenseeKey(u8 *key)
{
	klicensee_key = std::make_unique<u8[]>(0x10);
	memcpy(klicensee_key.get(), key, 0x10);
}

u8 *KeyVault::GetKlicenseeKey()
{
	return klicensee_key.get();
}

void rap_to_rif(unsigned char* rap, unsigned char* rif)
{
	int i;
	int round;
	aes_context aes;

	unsigned char key[0x10];
	unsigned char iv[0x10];
	memset(key, 0, 0x10);
	memset(iv, 0, 0x10);

	// Initial decrypt.
	aes_setkey_dec(&aes, RAP_KEY, 128);
	aes_crypt_cbc(&aes, AES_DECRYPT, 0x10, iv, rap, key);

	// rap2rifkey round.
	for (round = 0; round < 5; ++round)
	{
		for (i = 0; i < 16; ++i)
		{
			int p = RAP_PBOX[i];
			key[p] ^= RAP_E1[p];
		}
		for (i = 15; i >= 1; --i)
		{
			int p = RAP_PBOX[i];
			int pp = RAP_PBOX[i - 1];
			key[p] ^= key[pp];
		}
		int o = 0;
		for (i = 0; i < 16; ++i)
		{
			int p = RAP_PBOX[i];
			unsigned char kc = key[p] - o;
			unsigned char ec2 = RAP_E2[p];
			if (o != 1 || kc != 0xFF)
			{
				o = kc < ec2 ? 1 : 0;
				key[p] = kc - ec2;
			} else if (kc == 0xFF)
			{
				key[p] = kc - ec2;
			} else
			{
				key[p] = kc;
			}
		}
	}

	memcpy(rif, key, 0x10);
}

