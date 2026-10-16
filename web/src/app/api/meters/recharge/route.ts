import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const { meter_id, amount } = await request.json();

    const parsedAmount = parseFloat(amount);
    if (!meter_id || isNaN(parsedAmount)) {
      return NextResponse.json({ error: 'Valid meter ID and amount required' }, { status: 400 });
    }

    // Get current meter
    const { data: meter, error: fetchErr } = await supabase
      .from('meters')
      .select('balance, pending_recharge')
      .eq('id', meter_id)
      .single();

    if (fetchErr || !meter) {
      return NextResponse.json({ error: 'Meter not found' }, { status: 404 });
    }

    const newPending = (parseFloat(meter.pending_recharge) || 0) + parsedAmount;

    const { error: updateErr } = await supabase
      .from('meters')
      .update({ pending_recharge: newPending })
      .eq('id', meter_id);

    if (updateErr) {
      return NextResponse.json({ error: updateErr.message }, { status: 500 });
    }

    // Record in ledger
    await supabase.from('ledger').insert({
      meter_id,
      type: parsedAmount >= 0 ? 'recharge' : 'deduction',
      amount: parsedAmount,
      balance_after: parseFloat(meter.balance) + parsedAmount,
      note: `Remote fund adjustment from dashboard`
    });

    return NextResponse.json({ success: true, pending_recharge: newPending });
  } catch (error: any) {
    console.error('API /meters/recharge error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
