#pragma once
#include <array>
namespace idas3::original::net_password_data {
// Read-only tables recovered from canonical Stage 3 image efda831f1212db54.
// 0C261648
inline constexpr std::array<unsigned,20> baseTimes{1272000u,1314000u,1194000u,1428000u,1260000u,1428000u,1500000u,1350000u,1596000u,4320000u,1314000u,1356000u,1236000u,1470000u,1302000u,1470000u,1542000u,1392000u,1638000u,4500000u};
// 0C261698
inline constexpr std::array<unsigned,32> permutationA{0u,1u,2u,3u,4u,5u,6u,7u,11u,17u,23u,30u,27u,28u,26u,8u,29u,15u,22u,10u,19u,20u,31u,16u,18u,12u,14u,13u,24u,21u,9u,25u};
// 0C261718
inline constexpr std::array<unsigned,32> permutationB{0u,1u,2u,3u,4u,5u,6u,11u,17u,23u,30u,27u,28u,7u,26u,8u,29u,15u,22u,10u,19u,20u,31u,16u,18u,12u,14u,13u,24u,21u,9u,25u};
// 0C261818
inline constexpr std::array<unsigned,14> order{2u,5u,8u,9u,11u,4u,1u,10u,0u,6u,3u,7u,12u,13u};
// 0C261888
inline constexpr std::array<unsigned,12> symbols{7u,10u,5u,1u,8u,0u,2u,9u,3u,4u,6u,11u};
// 0C2618E8
inline constexpr std::array<unsigned,256> substitution{170u,185u,81u,158u,45u,17u,250u,172u,79u,74u,88u,130u,80u,254u,110u,234u,246u,40u,197u,165u,227u,105u,129u,231u,94u,196u,83u,248u,55u,27u,169u,62u,122u,56u,201u,43u,64u,87u,18u,245u,34u,242u,207u,244u,210u,103u,42u,109u,157u,206u,57u,137u,102u,31u,239u,11u,85u,198u,2u,22u,141u,178u,148u,164u,84u,65u,200u,163u,116u,24u,214u,143u,123u,171u,71u,146u,41u,183u,48u,249u,59u,1u,228u,36u,126u,54u,230u,44u,179u,205u,177u,202u,156u,92u,224u,69u,186u,166u,225u,155u,89u,76u,25u,120u,212u,220u,243u,180u,219u,51u,150u,241u,63u,119u,135u,29u,182u,145u,8u,78u,5u,46u,53u,236u,61u,10u,82u,174u,90u,134u,58u,192u,39u,72u,121u,189u,14u,20u,167u,6u,154u,4u,199u,136u,127u,181u,93u,147u,30u,28u,60u,19u,223u,47u,218u,204u,13u,138u,237u,132u,106u,133u,77u,176u,159u,232u,208u,49u,115u,144u,52u,252u,193u,149u,168u,162u,95u,240u,9u,21u,75u,233u,96u,12u,247u,139u,190u,215u,213u,68u,113u,195u,111u,203u,38u,187u,99u,37u,7u,161u,153u,221u,3u,86u,216u,117u,131u,194u,73u,15u,142u,238u,175u,32u,107u,100u,108u,66u,184u,211u,173u,226u,191u,118u,229u,253u,70u,101u,125u,104u,152u,255u,91u,0u,217u,222u,114u,50u,97u,124u,33u,98u,112u,35u,16u,128u,251u,67u,160u,235u,151u,209u,188u,23u,26u,140u};
// 0C261518
inline constexpr std::array<unsigned,12> characters{48u,49u,50u,51u,52u,53u,54u,55u,56u,57u,42u,35u};
// 0C261548
inline constexpr std::array<unsigned,64> glyphCharacters{48u,49u,50u,51u,52u,53u,54u,55u,56u,57u,65u,66u,67u,68u,69u,70u,71u,72u,73u,74u,75u,76u,77u,78u,79u,80u,81u,82u,83u,84u,85u,86u,87u,88u,89u,90u,97u,98u,99u,100u,101u,102u,103u,104u,105u,106u,107u,108u,109u,110u,111u,112u,113u,114u,115u,116u,117u,118u,119u,120u,121u,122u,42u,35u};
}
