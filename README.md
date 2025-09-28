# Project Z : 연구소 탈환 작전     

대한상공회의소 AI시스템반도체설계과정 ARM Architecture 수업에서 진행한 프로젝트입니다.  
두 개의 각각 다른 역할을 하는 STM32 보드를 이용하였으며, 두 보드간 UART 통신으로 데이터를 주고 받았습니다.  

자세한 내용은 project_docs의 [게임설명서](https://github.com/yjm020500/GameProject_ProjectZ/blob/main/project_docs/%EA%B2%8C%EC%9E%84%EC%84%A4%EB%AA%85%EC%84%9C_v1.pdf)와 [발표자료](https://github.com/yjm020500/GameProject_ProjectZ/blob/main/project_docs/%EB%B0%9C%ED%91%9C%EC%9E%90%EB%A3%8C.pdf)를 확인해주세요.  

<br>

## Board Setup
<div align="center">
  <img width="1094" height="400" alt="image" src="https://github.com/user-attachments/assets/1c233831-d577-4426-96ce-31fc282e8614" />
</div>
두 STM32보드를 UART2 연결하고(UART1은 각각 computer연결), main_device역할을 하는 보드에  <br>
체력 표시용 M3-EXT(SC16IS752)를 SPI 통신으로 연결합니다.  

<br>

## Game Details
### Game Story  
좀비 아포칼립스 세계에서 연구소 장악을 위해 연구소 최심부 제어실로 침투하는 탑뷰 게임입니다.  
내부에 있는 좀비와 경비 로봇을 피해 열쇠를 얻어 총 5 stage를 clear해야 합니다.
<br>
<br>
### Game Play
**1. Main_device**  
  - 플레이어는 JogKey를 사용하여 이동가능하고, SW1으로 총쏘기, SW2로 단말기로 이동합니다.  
  - 각 stage는 아이템 박스 획득 후, 단말기를 통한 해킹으로 키를 획득한 후 맵 밖으로 이동하여 클리어 가능합니다.  
  - 플레이어의 체력은 M3-EXT(SC16IS752)을 통해 확인 가능합니다.  
  
**2. Gear_device**  
  - 단말기에서는 획득한 아이템 박스를 소모하여 해킹 미니 게임이 수행 가능합니다.  
  - 미니게임은 일정 시간 내에 화면의 화살표 방향대로 JogKey를 움직여야 하며,   
    실패시에도 아이템 박스가 사라지지 않습니다.

<br>

## 추가 기능
Menu에서 단말기로 이동 가능합니다.  
두 device 모두 Setting을 통해 2종류의 BGM 선택 또는 음소거 가능합니다.
