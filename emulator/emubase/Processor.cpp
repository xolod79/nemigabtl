/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.cpp
//

#include "stdafx.h"
#include "Processor.h"


// Timings ///////////////////////////////////////////////////////////
// ������� ��������� �������� �� ������ �. �. ���������, ������ "������������ ��������� ��" �1 1995.

const int TIMING_BRANCH =   16;  // 5.4 us - BR, BEQ etc.
const int TIMING_ILLEGAL = 144;
const int TIMING_WAIT   = 1140;  // 380 us - WAIT and RESET
const int TIMING_EMT    =   68;  // 22.8 us - IOT, BPT, EMT, TRAP - 42+5t
const int TIMING_RTI    =   40;  // 13.4 us - RTI and RTT - 24+2t
const int TIMING_RTS    =   32;  // 10.8 us
const int TIMING_NOP    =   12;  // 4 us - NOP and all commands without operands and with register operands
const int TIMING_SOB    =   20;  // 6.8 us
const int TIMING_BR     =   16;  // 5.4 us
const int TIMING_MARK   =   36;

const int TIMING_REGREG =   11;  // Base timing
const int TIMING_A[8]   = { 0, 12, 12, 20, 12, 20, 20, 28 };  // Source
const int TIMING_B[8]   = { 0, 20, 20, 32, 20, 32, 32, 40 };  // Destination
const int TIMING_AB[8]  = { 0, 16, 16, 24, 16, 24, 24, 32 };  // Source and destination are the same
const int TIMING_A2[8]  = { 0, 20, 20, 28, 20, 28, 28, 36 };
const int TIMING_DS[8]  = { 0, 32, 32, 40, 32, 40, 40, 48 };

#define TIMING_A1 TIMING_A
#define TIMING_DJ TIMING_A2

#define TIMING_DST (m_methsrc ? TIMING_AB : TIMING_B)
#define TIMING_CMP (m_methsrc ? TIMING_A1 : TIMING_A2)

uint16_t ASH_TIMING[8] =
{
    0x0029, 0x003D, 0x003D, 0x0049, 0x0041, 0x004D, 0x0055, 0x0062
};
uint16_t ASH_S_TIMING = 0x0008;

uint16_t ASHC_TIMING[8] =
{
    0x0039, 0x004E, 0x004D, 0x005A, 0x0051, 0x005D, 0x0066, 0x0072
};
uint16_t ASHC_S_TIMING = 0x0008;

uint16_t MUL_TIMING[8] =
{
    0x0034, 0x009B, 0x009B, 0x00A8, 0x009E, 0x00AC, 0x00B5, 0x00C0
};

uint16_t DIV_TIMING[8] =
{
    0x0020, 0x0088, 0x0087, 0x0094, 0x008B, 0x0098, 0x00A0, 0x00AD
};


//////////////////////////////////////////////////////////////////////


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = NULL;

#define RegisterMethodRef(/*uint16_t*/ opstart, /*uint16_t*/ opend, /*CProcessor::ExecuteMethodRef*/ methodref) \
    { \
        for (uint32_t opcode = (opstart); opcode <= (opend); opcode++) \
            m_pExecuteMethodMap[opcode] = (methodref); \
    }

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == NULL);
    m_pExecuteMethodMap = (CProcessor::ExecuteMethodRef*) ::calloc(65536, sizeof(CProcessor::ExecuteMethodRef));

    // ������� ��������� ������� �������� �� ����� ExecuteUNKNOWN
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN );

    RegisterMethodRef( 0000000, 0000000, &CProcessor::ExecuteHALT );
    RegisterMethodRef( 0000001, 0000001, &CProcessor::ExecuteWAIT );
    RegisterMethodRef( 0000002, 0000002, &CProcessor::ExecuteRTI );
    RegisterMethodRef( 0000003, 0000003, &CProcessor::ExecuteBPT );
    RegisterMethodRef( 0000004, 0000004, &CProcessor::ExecuteIOT );
    RegisterMethodRef( 0000005, 0000005, &CProcessor::ExecuteRESET );
    RegisterMethodRef( 0000006, 0000006, &CProcessor::ExecuteRTT );

    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP );
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS );  // RTS / RETURN

    RegisterMethodRef( 0000240, 0000257, &CProcessor::ExecuteCCC );

    RegisterMethodRef( 0000260, 0000277, &CProcessor::ExecuteSCC );

    RegisterMethodRef( 0000300, 0000377, &CProcessor::ExecuteSWAB );

    RegisterMethodRef( 0000400, 0000777, &CProcessor::ExecuteBR );
    RegisterMethodRef( 0001000, 0001377, &CProcessor::ExecuteBNE );
    RegisterMethodRef( 0001400, 0001777, &CProcessor::ExecuteBEQ );
    RegisterMethodRef( 0002000, 0002377, &CProcessor::ExecuteBGE );
    RegisterMethodRef( 0002400, 0002777, &CProcessor::ExecuteBLT );
    RegisterMethodRef( 0003000, 0003377, &CProcessor::ExecuteBGT );
    RegisterMethodRef( 0003400, 0003777, &CProcessor::ExecuteBLE );

    RegisterMethodRef( 0004000, 0004777, &CProcessor::ExecuteJSR );  // JSR / CALL

    RegisterMethodRef( 0005000, 0005077, &CProcessor::ExecuteCLR );
    RegisterMethodRef( 0005100, 0005177, &CProcessor::ExecuteCOM );
    RegisterMethodRef( 0005200, 0005277, &CProcessor::ExecuteINC );
    RegisterMethodRef( 0005300, 0005377, &CProcessor::ExecuteDEC );
    RegisterMethodRef( 0005400, 0005477, &CProcessor::ExecuteNEG );
    RegisterMethodRef( 0005500, 0005577, &CProcessor::ExecuteADC );
    RegisterMethodRef( 0005600, 0005677, &CProcessor::ExecuteSBC );
    RegisterMethodRef( 0005700, 0005777, &CProcessor::ExecuteTST );
    RegisterMethodRef( 0006000, 0006077, &CProcessor::ExecuteROR );
    RegisterMethodRef( 0006100, 0006177, &CProcessor::ExecuteROL );
    RegisterMethodRef( 0006200, 0006277, &CProcessor::ExecuteASR );
    RegisterMethodRef( 0006300, 0006377, &CProcessor::ExecuteASL );

    RegisterMethodRef( 0006400, 0006477, &CProcessor::ExecuteMARK );
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT );

    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV );
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP );
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT );
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC );
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS );
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD );

    RegisterMethodRef( 0070000, 0070777, &CProcessor::ExecuteMUL );
    RegisterMethodRef( 0071000, 0071777, &CProcessor::ExecuteDIV );
    RegisterMethodRef( 0072000, 0072777, &CProcessor::ExecuteASH );
    RegisterMethodRef( 0073000, 0073777, &CProcessor::ExecuteASHC );
    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR );
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB );

    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL );
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI );
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI );
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS );
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC );
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS );
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS );  // BCC
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO );   // BCS

    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT );
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP );

    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLR );  // CLRB
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOM );  // COMB
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINC );  // INCB
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDEC );  // DECB
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEG );  // NEGB
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADC );  // ADCB
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBC );  // SBCB
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTST );  // TSTB
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteROR );  // RORB
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROL );  // ROLB
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASR );  // ASRB
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASL );  // ASLB

    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS );
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS );

    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOV );  // MOVB
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMP );  // CMPB
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBIT );  // BITB
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBIC );  // BICB
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBIS );  // BISB
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB );
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = NULL;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor(CMotherboard* pBoard)
{
    ASSERT(pBoard != NULL);
    m_pBoard = pBoard;
    ::memset(m_R, 0, sizeof(m_R));
    m_psw = 0340;
    m_okStopped = true;  m_haltmode = false;
    m_internalTick = 0;
    m_waitmode = false;
    m_stepmode = false;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_HALTrq = m_RPL2rq = m_EVNTrq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;

    m_instruction = m_instructionpc = 0;
    m_regsrc = m_methsrc = 0;
    m_regdest = m_methdest = 0;
    m_addrsrc = m_addrdest = 0;
    memset(m_virq, 0, sizeof(m_virq));

    memset(m_eisregs, 0, sizeof(m_eisregs));
}

void CProcessor::Start()
{
    m_okStopped = false;  m_haltmode = true;

    m_stepmode = false;
    m_waitmode = false;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_HALTrq = m_RPL2rq = m_EVNTrq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));

    // "Turn On" interrupt processing
    uint16_t pc = GetWord(0160006);
    SetPC(pc);
    uint16_t ps = GetWord(0160010);
    SetPSW(ps);
    m_internalTick = 1000000;  // ���������� ������ �� ��������� ���������� (�������� � �������)
}
void CProcessor::Stop()
{
    m_okStopped = true;

    m_stepmode = false;
    m_waitmode = false;
    m_psw = 0340;
    m_internalTick = 0;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_HALTrq = m_RPL2rq = m_EVNTrq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));
}

void CProcessor::Execute()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }
    m_internalTick = TIMING_ILLEGAL;  // ANYTHING UNKNOWN

    m_RPLYrq = false;

    if (!m_waitmode)
    {
        m_instructionpc = m_R[7];  // Store address of the current instruction
        FetchInstruction();  // Read next instruction from memory
        if (!m_RPLYrq)
        {
            TranslateInstruction();  // Execute next instruction
            if (m_internalTick > 0) m_internalTick--;  // Count current tick too
        }
    }

    if (m_stepmode)
        m_stepmode = false;
    else if (m_instruction == PI_RTT && (GetPSW() & PSW_T))
    {
        // Skip interrupt processing for RTT with T bit set
    }
    else  // Processing interrupts
    {
        for (;;)
        {
            m_TBITrq = (m_psw & 020);  // T-bit

            // Calculate interrupt vector and mode accoding to priority
            uint16_t intrVector = 0;
            //bool currMode = m_haltmode;  // Current processor mode: true = HALT mode, false = USER mode
            bool intrMode = false;  // true = HALT mode interrupt, false = USER mode interrupt
            if (m_HALTrq)  // HALT command
            {
                intrVector = 0002;  intrMode = true;
                m_HALTrq = false;
            }
            else if (m_BPT_rq)  // BPT command
            {
                intrVector = 0000014;  intrMode = false;
                m_BPT_rq = false;
            }
            else if (m_IOT_rq)  // IOT command
            {
                intrVector = 0000020;  intrMode = false;
                m_IOT_rq = false;
            }
            else if (m_EMT_rq)  // EMT command
            {
                intrVector = 0000030;  intrMode = false;
                m_EMT_rq = false;
            }
            else if (m_TRAPrq)  // TRAP command
            {
                intrVector = 0000034;  intrMode = false;
                m_TRAPrq = false;
            }
            else if (m_RPLYrq)  // ���������
            {
                intrVector = 0000004;  intrMode = false;
                m_RPLYrq = false;
            }
            //else if (m_RPL2rq)  // ������� ���������, priority 1
            //{
            //    intrVector = 0174;  intrMode = true;
            //    m_RPL2rq = false;
            //}
            else if (m_RSVDrq)  // Reserved command, priority 2
            {
                intrVector = 000010;  intrMode = false;
                m_RSVDrq = false;
            }
            else if (m_TBITrq && (!m_waitmode))  // T-bit
            {
                intrVector = 000014;  intrMode = false;
                m_TBITrq = false;
            }
            else if (m_EVNTrq && (m_psw & 0200) != 0200)  // EVNT signal
            {
                intrVector = 0000100;  intrMode = false;
                m_EVNTrq = false;
            }
            else if (m_virqrq > 0 && (m_psw & 0200) != 0200)  // VIRQ, priority 7
            {
                intrMode = false;
                for (int irq = 0; irq <= 15; irq++)
                {
                    if (m_virq[irq] != 0)
                    {
                        intrVector = m_virq[irq];
                        m_virq[irq] = 0;
                        m_virqrq--;
                        break;
                    }
                }
                if (intrVector == 0) m_virqrq = 0;
            }

            if (intrVector == 0)
                break;  // No more unmasked interrupts

            m_waitmode = false;

            if (intrMode)  // HALT mode interrupt
            {
                uint16_t selVector = 0160000;
                intrVector |= selVector;

                uint16_t oldpsw = m_psw;
                m_haltmode = true;

                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), oldpsw);
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetPC());

                SetPC(GetWord(intrVector));
                m_psw = GetWord(intrVector + 2) & 0377;
#if !defined(PRODUCT)
                if (intrVector == 0160002)  // HALT
                {
                    uint16_t port170006 = m_pBoard->GetPortView(0170006);
                    if (port170006 & 002000)  // keyboard
                    {
                        uint8_t keybyte = (uint8_t)(port170006 & 255);
                        DebugLogFormat(_T("CPU HALT interrupt vector=%06o PC=%06o PSW=%06o 170006=%06o %C\r\n"), intrVector, GetPC(), GetPSW(), port170006, keybyte >= 32 && keybyte < 128 ? (char)keybyte : ' ');
                    }
                }
                //if (m_pBoard->GetTrace() & TRACE_CPUINT)
                //{
                //    if (intrVector == 0160002)  // HALT
                //    {
                //        uint16_t port170006 = m_pBoard->GetPortView(0170006);
                //        uint8_t keybyte = (uint8_t)(port170006 & 255);
                //        DebugLogFormat(_T("CPU HALT interrupt vector=%06o PC=%06o PSW=%06o 170006=%06o %C\r\n"), intrVector, GetPC(), GetPSW(), port170006, keybyte >= 32 && keybyte < 128 ? (char)keybyte : ' ');
                //    }
                //    else
                //    {
                //        DebugLogFormat(_T("CPU interrupt vector=%06o PC=%06o PSW=%06o\r\n"), intrVector, GetPC(), GetPSW());
                //    }
                //}
#endif
            }
            else  // USER mode interrupt
            {
                uint16_t oldpsw = m_psw;
                m_haltmode = false;

                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), oldpsw);
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetPC());

                SetPC(GetWord(intrVector));
                m_psw = GetWord(intrVector + 2) & 0377;

//                if (m_pBoard->GetTrace() & TRACE_CPUINT)
//                {
//                    if (intrVector != 000020 && intrVector != 000030 && intrVector != 000034)  // skip IOT/EMT/TRAP
//                        DebugLogFormat(_T("CPU interrupt vector=%06o PC=%06o PSW=%06o\r\n"), intrVector, GetPC(), GetPSW());
//                }
            }
        }  // end while
    }
}

void CProcessor::TickEVNT()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_EVNTrq = true;
}

void CProcessor::InterruptVIRQ(int que, uint16_t interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
    // if (m_virqrq == 1)
    // {
    //  DebugPrintFormat(_T("Lost VIRQ %d %d\r\n"), m_virq, interrupt);
    // }
    m_virqrq += 1;
    m_virq[que] = interrupt;
}

void CProcessor::MemoryError()
{
    m_RPLYrq = true;
}


//////////////////////////////////////////////////////////////////////


// ���������� ������ ��������, � ����������� �� ������ ���������
//   meth - ����� ���������
//   reg  - ����� ��������
uint16_t CProcessor::CalculateOperAddrSrc (int meth, int reg)
{
    uint16_t arg;

    switch (meth)
    {
    case 0:  // R0,     PC
        return GetReg(reg);
    case 1:  // (R0),   (PC)
        return GetReg(reg);
    case 2:  // (R0)+,  #012345
        //if(reg==7) // is it immediate?
        //    arg = GetWord(GetReg(reg));
        //else
        arg = GetReg(reg);
        if ((m_instruction & 0100000) && (reg < 6))
            SetReg(reg, GetReg(reg) + 1);
        else
            SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 3:  // @(R0)+, @#012345
        //if(reg==7) //abs index
        //    arg =  GetWord(GetWord(GetReg(reg))) ;
        //else
        arg =  GetWord(GetReg(reg)) ;
        //if ((m_instruction & 0100000)&&(reg!=7))
        //    SetReg(reg, GetReg(reg) + 1);
        //else
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 4:  // -(R0),  -(PC)
        if ((m_instruction & 0100000) && (reg < 6))
            SetReg(reg, GetReg(reg) - 1);
        else
            SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5:  // @-(R0), @-(PC)
        //      if (m_instruction & 0100000)
        //          SetReg(reg, GetReg(reg) - 1);
        //      else
        SetReg(reg, GetReg(reg) - 2);
        return  GetWord(GetReg(reg));
    case 6:    // 345(R0),  345
        {
            uint16_t pc = 0;
            //if(reg==7) //relative direct
            //    pc = GetWord(GetWordExec( GetPC() ));
            //else
            pc = (GetWordExec( GetPC() ));

            SetPC( GetPC() + 2 );
            arg = (uint16_t)(pc + GetReg(reg));
            return arg;
        }
    case 7:    // @345(R0),@345
        {
            uint16_t pc;
            //if(reg==7) //relative direct
            //    pc = GetWord(GetWordExec( GetPC() ));
            //else
            pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            arg = ( GetWord(pc + GetReg(reg)) );
            return arg;
        }
    }

    return 0;
}

uint16_t CProcessor::CalculateOperAddr (int meth, int reg)
{
    uint16_t arg;
    switch (meth)
    {
    case 0:  // R0,     PC
        return GetReg(reg);
    case 1:  // (R0),   (PC)
        return GetReg(reg);
    case 2:  // (R0)+,  #012345
        //if(reg==7) // is it immediate?
        //    arg = GetWord(GetReg(reg));
        //else
        arg = GetReg(reg);
        if ((m_instruction & 0100000) && (reg < 6))
            SetReg(reg, GetReg(reg) + 1);
        else
            SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 3:  // @(R0)+, @#012345
        //if(reg==7) //abs index
        //arg =  GetWord(GetWord(GetReg(reg))) ;
        //else
        arg =  GetWord(GetReg(reg)) ;
        //if ((m_instruction & 0100000)&&(reg!=7))
        //    SetReg(reg, GetReg(reg) + 1);
        //else
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 4:  // -(R0),  -(PC)
        if ((m_instruction & 0100000) && (reg < 6))
            SetReg(reg, GetReg(reg) - 1);
        else
            SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5:  // @-(R0), @-(PC)
        //if (m_instruction & 0100000)
        //    SetReg(reg, GetReg(reg) - 1);
        //else
        SetReg(reg, GetReg(reg) - 2);
        return  GetWord(GetReg(reg));
    case 6:    // 345(R0),  345
        {
            uint16_t pc = 0;
            //if(reg==7) //relative direct
            // pc = GetWord(GetWordExec( GetPC() ));
            //else
            pc = (GetWordExec( GetPC() ));

            SetPC( GetPC() + 2 );
            arg = (uint16_t)(pc + GetReg(reg));
            return arg;
        }
    case 7:    // @345(R0),@345
        {
            uint16_t pc = 0;
            //if(reg==7)
            //    pc = GetWord(GetWordExec( GetPC() ));
            //else
            pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            arg = ( GetWord(pc + GetReg(reg)) );
            return arg;
        }
    }

    return 0;
}


uint8_t CProcessor::GetByteSrc ()
{
    if (m_methsrc == 0)
        return (uint8_t) GetReg(m_regsrc) & 0377;
    else
        return GetByte( m_addrsrc );
}
uint8_t CProcessor::GetByteDest ()
{
    if (m_methdest == 0)
        return (uint8_t) GetReg(m_regdest);
    else
        return GetByte( m_addrdest );
}

void CProcessor::SetByteDest (uint8_t byte)
{
    if (m_methdest == 0)
    {
        if (byte & 0200)
            SetReg(m_regdest, 0xff00 | byte);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0xff00) | byte);
    }
    else
        SetByte( m_addrdest, byte );
}

uint16_t CProcessor::GetWordSrc ()
{
    if (m_methsrc == 0)
    {
        return GetReg(m_regsrc);
    }
    else
        return GetWord( m_addrsrc );
}
uint16_t CProcessor::GetWordDest ()
{
    if (m_methdest == 0)
        return GetReg(m_regdest);
    else
        return GetWord( m_addrdest );
}

void CProcessor::SetWordDest (uint16_t word)
{
    if (m_methdest == 0)
        SetReg(m_regdest, word);
    else
        SetWord( (m_addrdest), word );
}

uint16_t CProcessor::GetDstWordArgAsBranch ()
{
    int reg = GetDigit(m_instruction, 0);
    int meth = GetDigit(m_instruction, 1);
    uint16_t arg;

    switch (meth)
    {
    case 0:  // R0,     PC
        ASSERT(0);
        return 0;
    case 1:  // (R0),   (PC)
        return GetReg(reg);
    case 2:  // (R0)+,  #012345
        arg = GetReg(reg);
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 3:  // @(R0)+, @#012345
        arg = GetWord( GetReg(reg) );
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 4:  // -(R0),  -(PC)
        SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5:  // @-(R0), @-(PC)
        SetReg(reg, GetReg(reg) - 2);
        return GetWord( GetReg(reg) );
    case 6:    // 345(R0),  345
        {
            uint16_t pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return (uint16_t)(pc + GetReg(reg));
        }
    case 7:    // @345(R0),@345
        {
            uint16_t pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return GetWord( (uint16_t)(pc + GetReg(reg)) );
        }
    }

    return 0;
}

uint16_t CProcessor::GetEisReg(int reg) const
{
    ASSERT(reg >= 0 && reg < 3);
    return m_eisregs[reg];
}


//////////////////////////////////////////////////////////////////////

static bool TraceStarted = true;//DEBUG
void CProcessor::FetchInstruction()
{
    // ��������� ��������� ����������
    uint16_t pc = GetPC();
    pc = pc & ~1;

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);

//    uint16_t address = GetPC() - 2;
//    const uint16_t TraceStartAddress = 0000000;
//    //if (!TraceStarted && address == TraceStartAddress) TraceStarted = true;
//    if (TraceStarted /*&& address < 0160000*/) {
//        uint16_t data[4];
//        for (int i = 0; i < 4; i++)
//            data[i] = GetWord(address + i * 2);
//        TCHAR strInstr[8];
//        TCHAR strArg[32];
//        DisassembleInstruction(data, address, strInstr, strArg);
//        DebugLogFormat(_T("%06o: %s\t%s\n"), address, strInstr, strArg);
//        //DebugLogFormat(_T("%s %06o: %s\t%s\n"), IsHaltMode()?_T("HALT"):_T("USER"), address, strInstr, strArg);
//    }
}

void CProcessor::TranslateInstruction ()
{
    // Prepare values to help decode the command
    m_regdest  = (uint8_t)GetDigit(m_instruction, 0);
    m_methdest = (uint8_t)GetDigit(m_instruction, 1);
    m_regsrc   = (uint8_t)GetDigit(m_instruction, 2);
    m_methsrc  = (uint8_t)GetDigit(m_instruction, 3);

    // Find command implementation using the command map
    ExecuteMethodRef methodref = m_pExecuteMethodMap[m_instruction];
    (this->*methodref)();  // Call command implementation method
}

void CProcessor::ExecuteUNKNOWN ()  // ��� ����� ���������� - ������ ���������� TRAP 10
{
    DebugLogFormat(_T(">>Invalid OPCODE = %06o at %06o\r\n"), m_instruction, m_instructionpc);

    m_RSVDrq = true;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
    m_waitmode = true;

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteHALT ()  // HALT - �������
{
    m_HALTrq = true;
}

void CProcessor::ExecuteRTI ()  // RTI - ������� �� ����������
{
    uint16_t new_psw;
    SetReg(7, GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );

    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetPSW(new_psw & 0377);

    SetSP( GetSP() + 2 );
    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
    m_BPT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
    m_IOT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices
{
    m_pBoard->ResetDevices();  // INIT signal

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteRTT ()  // RTT - return from trace trap
{
    uint16_t new_psw;
    SetPC( GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );

    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    SetPSW(new_psw & 0377);

    //m_psw |= PSW_T; // set the trap flag ???
    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - ������� �� ���������
{
    SetPC(GetReg(m_regdest));
    SetReg(m_regdest, GetWord(GetSP()));
    SetSP(GetSP() + 2);

    m_internalTick = TIMING_RTS;
}

void CProcessor::ExecuteNOP ()  // NOP - ��� ��������
{
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteCCC ()
{
    SetLPSW(GetLPSW() &  ~((uint8_t)(m_instruction & 0xff) & 017));
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteSEZ ()
{
    SetZ(true);
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteSCC ()
{
    SetLPSW(GetLPSW() |  ((uint8_t)(m_instruction & 0xff) & 017));
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // ������������ ����� ���������
    {
        m_RPLYrq = true;

        m_internalTick = TIMING_EMT;
    }
    else
    {
        SetPC(GetWordAddr(m_methdest, m_regdest));

        m_internalTick = TIMING_DJ[m_methdest];
    }
}

void CProcessor::ExecuteSWAB ()
{
    uint16_t ea = 0;
    uint16_t dst;

    dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
    dst = ((dst >> 8) & 0377) | (dst << 8);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    SetN((dst & 0200) != 0);
    SetZ(!(dst & 0377));
    SetV(0);
    SetC(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCLR ()  // CLR
{
    if (m_instruction & 0100000)
    {
        SetN(0);
        SetZ(1);
        SetV(0);
        SetC(0);

        if (m_methdest)
            SetByte(GetByteAddr(m_methdest, m_regdest), 0);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400));

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        SetN(0);
        SetZ(1);
        SetV(0);
        SetC(0);

        if (m_methdest)
            SetWord(GetWordAddr(m_methdest, m_regdest), 0);
        else
            SetReg(m_regdest, 0);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteCOM ()  // COM
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = dst ^ 0377;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);
        SetC(1);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        dst = dst ^ 0177777;
        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);
        SetC(1);
        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteINC ()  // INC - ���������
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = dst + 1;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(dst == 0200);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst + 1;
        SetN(dst >> 15);
        SetZ(!dst);
        SetV(dst == 0100000);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteDEC ()  // DEC - ���������
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = dst - 1;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(dst == 0177);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst - 1;
        SetN(dst >> 15);
        SetZ(!dst);
        SetV(dst == 077777);
        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteNEG ()
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = 0 - dst;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(dst == 0200);
        SetC(!GetZ());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = 0 - dst;

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(dst == 0100000);
        SetC(!GetZ());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteADC ()  // ADC{B}
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = dst + (GetC() ? 1 : 0);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);


        SetN(dst >> 7);
        SetZ(!dst);
        SetV(GetC() && (dst == 0200));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        dst = dst + (GetC() ? 1 : 0);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);


        SetN(dst >> 15);
        SetZ(!dst);
        SetV(GetC() && (dst == 0100000));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteSBC ()  // SBC{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        dst = dst - (GetC() ? 1 : 0);

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(GetC() && (dst == 0177));
        SetC(GetC() && (dst == 0377));

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t dst;
        dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst - (GetC() ? 1 : 0);

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(GetC() && (dst == 077777));
        SetC(GetC() && (dst == 0177777));

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteTST ()  // TST{B} - test
{
    if (m_instruction & 0100000)
    {
        uint8_t dst;

        if (m_methdest)
            dst = GetByte(GetByteAddr(m_methdest, m_regdest));
        else
            dst = GetLReg(m_regdest);

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);
        SetC(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
    }
    else
    {
        uint16_t dst;

        dst = m_methdest ? GetWord(GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);
        SetC(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
    }
}

void CProcessor::ExecuteROR ()  // ROR{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t dst;

        if (m_methdest)
            src = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src = GetLReg(m_regdest);

        dst = (src >> 1) | (GetC() ? 0200 : 0);

        SetN(dst >> 7);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t dst;

        src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = (src >> 1) | (GetC() ? 0100000 : 0);

        SetN(dst >> 15);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteROL ()  // ROL{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t dst;

        if (m_methdest)
            src = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src = GetLReg(m_regdest);

        dst = (src << 1) | (GetC() ? 1 : 0);

        SetN(dst >> 7);
        SetZ(!dst);
        SetC(src >> 7);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t dst;

        src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = (src << 1) | (GetC() ? 1 : 0);

        SetN(dst >> 15);
        SetZ(!dst);
        SetC(src >> 15);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteASR ()  // ASR{B}
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t dst;

        if (m_methdest)
            src = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src = GetLReg(m_regdest);

        dst = (src >> 1) | (src & 0200);
        SetN(dst >> 7);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t dst;

        src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        dst = (src >> 1) | (src & 0100000);
        SetN(dst >> 15);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteASL ()  // ASL{B}
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t dst;

        if (m_methdest)
            src = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src = GetLReg(m_regdest);

        dst = (src << 1) & 0377;
        SetN(dst >> 7);
        SetZ(!dst);
        SetC(src >> 7);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t dst;
        src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        dst = src << 1;
        SetN(dst >> 15);
        SetZ(!dst);
        SetC(src >> 15);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteSXT ()  // SXT - sign-extend
{
    if (m_methdest)
        SetWord(GetWordAddr(m_methdest, m_regdest), GetN() ? 0177777 : 0);
    else
        SetReg(m_regdest, GetN() ? 0177777 : 0); //sign extend

    SetZ(!GetN());
    SetV(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMTPS ()  // MTPS - move to PS
{
    uint8_t dst;

    if (m_methdest)
        dst = GetByte(GetByteAddr(m_methdest, m_regdest));
    else
        dst = GetLReg(m_regdest);

    if (GetPSW() & 0400) //in halt?
    {
        //allow everything
        SetPSW((GetPSW() & 0400) | dst);
    }
    else
    {
        SetPSW((GetPSW() & 0420) | (dst & 0357));  // preserve T
    }

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMFPS ()  // MFPS - move from PS
{
    uint8_t psw = GetPSW() & 0377;
    if (m_methdest)
        SetByte(GetByteAddr(m_methdest, m_regdest), psw);
    else
        SetReg(m_regdest, (char)psw); //sign extend
    SetN(psw & 0200);
    SetZ(psw == 0);
    SetV(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteBR ()
{
    SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    m_internalTick = TIMING_BR;
}

void CProcessor::ExecuteBNE ()
{
    if (! GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBEQ ()
{
    if (GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGE ()
{
    if (GetN() == GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLT ()
{
    if (GetN() != GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGT ()
{
    if (! ((GetN() != GetV()) || GetZ()))
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLE ()
{
    if ((GetN() != GetV()) || GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBPL ()
{
    if (! GetN())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBMI ()
{
    if (GetN())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHI ()
{
    if (! (GetZ() || GetC()))
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLOS ()
{
    if (GetZ() || GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVC ()
{
    if (! GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVS ()
{
    if (GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHIS ()
{
    if (! GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLO ()
{
    if (GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteXOR ()  // XOR
{
    uint16_t dst;
    uint16_t ea = 0;

    dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
    dst = dst ^ GetReg(m_regsrc);

    SetN(dst >> 15);
    SetZ(!dst);
    SetV(0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_REGREG + TIMING_A2[m_methdest];
}

void CProcessor::ExecuteMUL ()  // MUL
{
    uint16_t dst = GetReg(m_regsrc);
    uint16_t src, ea = 0;
    int res;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = m_methdest ? GetWord(ea) : GetReg(m_regdest);
    if (m_RPLYrq) return;

    res = (signed short)dst * (signed short)src;

    uint16_t hiword = HIWORD(res);
    uint16_t loword = LOWORD(res);
    SetReg(m_regsrc, hiword);
    SetReg(m_regsrc | 1, loword);

    if (res < 0) new_psw |= PSW_N;
    if (res == 0) new_psw |= PSW_Z;
    if ((res > 32767) || (res < -32768)) new_psw |= PSW_C;
    if (res == 32767) new_psw |= PSW_C;  // Nemiga bug in MUL
    SetLPSW(new_psw);
    m_internalTick = MUL_TIMING[m_methdest];

    m_eisregs[0] = loword;
    m_eisregs[1] = hiword;
    m_eisregs[2] = ((new_psw & 15) << 12) | 0007760 | (new_psw & 15);
}
void CProcessor::ExecuteDIV ()  // DIV
{
    //����� ���� ������� ���
    uint16_t ea = 0;
    int longsrc;
    int res, res1, src2;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src2 = (int)(signed short)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;

    longsrc = (int)MAKELONG(GetReg(m_regsrc | 1), GetReg(m_regsrc));

    m_internalTick = DIV_TIMING[m_methdest];

    m_eisregs[0] = m_eisregs[1] = 0;
    m_eisregs[2] = 0047764;

    if (src2 == 0)
    {
        new_psw |= (PSW_V | PSW_C); //���� ����� �� 0 -- �� ������������� V � C
        SetLPSW(new_psw);
        return;
    }
    if ((longsrc == 020000000000) && (src2 == -1))
    {
        new_psw |= PSW_V; // �������������, ��������
        SetLPSW(new_psw);
        return;
    }

    res = longsrc / src2;
    res1 = longsrc % src2;

    if ((res > 32767) || (res < -32768))
    {
        new_psw |= PSW_V; // �������������, ��������
        SetLPSW(new_psw);
        return;
    }

    SetReg(m_regsrc | 1, res1 & 0177777);
    SetReg(m_regsrc, res & 0177777);

    if (res < 0) new_psw |= PSW_N;
    if (res == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);
}
void CProcessor::ExecuteASH ()  // ASH
{
    uint16_t ea = 0;
    short src;
    short dst;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = (short)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
    src |= (src & 040) ? 0177700 : 0;
    dst = (short)GetReg(m_regsrc);

    m_internalTick = ASH_TIMING[m_methdest];

    if (src >= 0)
    {
        while (src--)
        {
            if (dst & 0100000) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst <<= 1;
            if ((dst < 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
            m_internalTick = m_internalTick + ASH_S_TIMING;
        }
    }
    else
    {
        while (src++)
        {
            if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst >>= 1;
            m_internalTick = m_internalTick + ASH_S_TIMING;
        }
    }

    SetReg(m_regsrc, dst);

    if (dst < 0) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);

    m_eisregs[0] = m_eisregs[1] = 0;
    m_eisregs[2] = 0047764;
}
void CProcessor::ExecuteASHC ()  // ASHC
{
    uint16_t ea = 0;
    short src;
    long dst;
    uint8_t new_psw = GetLPSW() & 0xF0;

    if (m_methdest) ea = GetWordAddr(m_methdest, m_regdest);
    if (m_RPLYrq) return;
    src = (short)(m_methdest ? GetWord(ea) : GetReg(m_regdest));
    if (m_RPLYrq) return;
    src &= 0x3F;
    src |= (src & 040) ? 0177700 : 0;
    dst = MAKELONG(GetReg(m_regsrc | 1), GetReg(m_regsrc));
    m_internalTick = ASHC_TIMING[m_methdest];
    if (src >= 0)
    {
        while (src--)
        {
            if (dst & 0x80000000L) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst <<= 1;
            if ((dst < 0) != ((new_psw & PSW_C) != 0)) new_psw |= PSW_V;
            m_internalTick = m_internalTick + ASHC_S_TIMING;
        }
    }
    else
    {
        while (src++)
        {
            if (dst & 1) new_psw |= PSW_C; else new_psw &= ~PSW_C;
            dst >>= 1;
            m_internalTick = m_internalTick + ASHC_S_TIMING;
        }
    }

    SetReg(m_regsrc, HIWORD(dst));
    SetReg(m_regsrc | 1, LOWORD(dst));

    SetN(dst < 0);
    SetZ(dst == 0);
    if (dst < 0) new_psw |= PSW_N;
    if (dst == 0) new_psw |= PSW_Z;
    SetLPSW(new_psw);

    m_eisregs[0] = m_eisregs[1] = 0;
    m_eisregs[2] = 0047764;
}

void CProcessor::ExecuteSOB ()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
    uint16_t dst = GetReg(m_regsrc);

    --dst;
    SetReg(m_regsrc, dst);

    if (dst)
    {
        SetPC(GetPC() - (m_instruction & (uint16_t)077) * 2 );
    }

    m_internalTick = TIMING_SOB;
}

void CProcessor::ExecuteMOV ()
{
    if (m_instruction & 0100000) // MOVB
    {
        uint8_t dst = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : GetLReg(m_regsrc);

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetByte(GetByteAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, (dst & 0200) ? (0177400 | dst) : dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else  // MOV
    {
        uint16_t dst = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetWord(GetWordAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteCMP ()
{
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t src2;
        uint8_t dst;

        if (m_methsrc)
            src = GetByte(GetByteAddr(m_methsrc, m_regsrc));
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
            src2 = GetByte(GetByteAddr(m_methdest, m_regdest));
        else
            src2 = GetLReg(m_regdest);

        dst = src - src2;
        SetN( CheckForNegative((uint8_t)(src - src2)) );
        SetZ( CheckForZero((uint8_t)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t src2;
        uint16_t dst;

        src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetWord(GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src - src2;

        SetN( CheckForNegative ((uint16_t)(src - src2)) );
        SetZ( CheckForZero ((uint16_t)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
}

void CProcessor::ExecuteBIT ()  // BIT{B} - bit test
{
    uint16_t ea;
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t src2;
        uint8_t dst;

        if (m_methsrc)
            src = GetByte(GetByteAddr(m_methsrc, m_regsrc));
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
            src2 = GetByte(GetByteAddr(m_methdest, m_regdest));
        else
            src2 = GetLReg(m_regdest);

        dst = src2 & src;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t src2;
        uint16_t dst;

        src  = m_methsrc  ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src2 & src;

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
}

void CProcessor::ExecuteBIC ()  // BIC{B} - bit clear
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t src2;
        uint8_t dst;

        if (m_methsrc)
            src = GetByte(GetByteAddr(m_methsrc, m_regsrc));
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
            src2 = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src2 = GetLReg(m_regdest);

        dst = src2 & (~src);

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t src2;
        uint16_t dst;

        src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src2 & (~src);

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteBIS ()  // BIS{B} - bit set
{
    uint16_t ea = 0;
    if (m_instruction & 0100000)
    {
        uint8_t src;
        uint8_t src2;
        uint8_t dst;

        if (m_methsrc)
            src = GetByte(GetByteAddr(m_methsrc, m_regsrc));
        else
            src = GetLReg(m_regsrc);

        if (m_methdest)
            src2 = GetByte(ea = GetByteAddr(m_methdest, m_regdest));
        else
            src2 = GetLReg(m_regdest);

        dst = src2 | src;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else
    {
        uint16_t src;
        uint16_t src2;
        uint16_t dst;

        src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src2 | src;

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteADD ()  // ADD
{
    uint16_t src;
    uint16_t src2;
    signed short dst;
    signed long dst2;
    uint16_t ea = 0;

    src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
    src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    SetN(CheckForNegative ((uint16_t)(src2 + src)));
    SetZ(CheckForZero ((uint16_t)(src2 + src)));
    SetV(CheckAddForOverflow (src2, src));
    SetC(CheckAddForCarry (src2, src));


    dst = src2 + src;
    dst2 = (short)src2 + (short)src;


    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteSUB ()
{
    uint16_t src;
    uint16_t src2;
    uint16_t dst;
    uint16_t ea = 0;

    src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
    src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    SetN(CheckForNegative ((uint16_t)(src2 - src)));
    SetZ(CheckForZero ((uint16_t)(src2 - src)));
    SetV(CheckSubForOverflow (src2, src));
    SetC(CheckSubForCarry (src2, src));

    dst = src2 - src;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteEMT ()  // EMT - emulator trap
{
    m_EMT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteTRAP ()
{
    m_TRAPrq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteJSR ()  // JSR - Jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    //int meth = GetDigit(m_instruction, DST + 1);
    if (m_methdest == 0)
    {
        // ������������ ����� ���������
        m_RPLYrq = true;
        m_internalTick = TIMING_EMT;
    }
    else
    {
        uint16_t dst;
        //uint16_t pc = GetDstWordArgAsBranch();
        dst = GetWordAddr(m_methdest, m_regdest);

        SetSP( GetSP() - 2 );

        SetWord( GetSP(), GetReg(m_regsrc) );

        SetReg(m_regsrc, GetPC());

        SetPC(dst);

        m_internalTick = TIMING_DS[m_methdest];
    }
}

void CProcessor::ExecuteMARK ()  // MARK
{
    SetSP( GetPC() + (m_instruction & 0x003F) * 2 );
    SetPC( GetReg(5) );
    SetReg(5, GetWord( GetSP() ));
    SetSP( GetSP() + 2 );

    m_internalTick = TIMING_MARK;
}

//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2 bytes        PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2 bytes        Stopped flag: !0 - stopped, 0 - not stopped

void CProcessor::SaveToImage(uint8_t* pImage)
{
    uint16_t* pwImage = (uint16_t*) pImage;
    // PSW
    *pwImage++ = m_psw;
    // Registers R0..R7
    ::memcpy(pwImage, m_R, 2 * 8);
    pwImage += 2 * 8;
    // Saved PC and PSW
    *pwImage++ = 0;  //m_savepc;
    *pwImage++ = 0;  //m_savepsw;
    // Stopped flag
    *pwImage++ = (m_okStopped ? 1 : 0);
    *pwImage++ = (m_waitmode ? 1 : 0);
    *pwImage++ = (m_stepmode ? 1 : 0);
    *pwImage++ = (m_haltmode ? 1 : 0);
}

void CProcessor::LoadFromImage(const uint8_t* pImage)
{
    Stop();  // Reset internal state

    uint16_t* pwImage = (uint16_t*) pImage;
    // PSW
    m_psw = *pwImage++;
    // Registers R0..R7
    ::memcpy(m_R, pwImage, 2 * 8);
    // Saved PC and PSW - skip
    pwImage++;
    pwImage++;
    // Stopped flag
    m_okStopped = (*pwImage++ != 0);
    m_waitmode = (*pwImage++ != 0);
    m_stepmode = (*pwImage++ != 0);
    m_haltmode = (*pwImage++ != 0);
}

uint16_t CProcessor::GetWordAddr (uint8_t meth, uint8_t reg)
{
    uint16_t addr;

    addr = 0;

    switch (meth)
    {
    case 1:   //(R)
        addr = GetReg(reg);
        break;
    case 2:   //(R)+
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        break;
    case 3:  //@(R)+
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4: //-(R)
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        break;
    case 5: //@-(R)
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        addr = GetWord(addr);
        break;
    }
    return addr;
}

uint16_t CProcessor::GetByteAddr (uint8_t meth, uint8_t reg)
{
    uint16_t addr, delta;

    addr = 0;
    switch (meth)
    {
    case 1:
        addr = GetReg(reg);
        break;
    case 2:
        delta = 1 + (reg >= 6);
        addr = GetReg(reg);
        SetReg(reg, addr + delta);
        break;
    case 3:
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4:
        delta = 1 + (reg >= 6);
        SetReg(reg, GetReg(reg) - delta);
        addr = GetReg(reg);
        break;
    case 5:
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        addr = GetWord(addr);
        break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
