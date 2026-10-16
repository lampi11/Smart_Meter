import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const ownerId = searchParams.get('ownerId');

    // Fetch meters with optional owner filter
    let query = supabase
      .from('meters')
      .select(`
        id,
        device_id,
        serial_number,
        label,
        balance,
        tariff_grid,
        relay_on,
        last_seen_at,
        created_at,
        properties ( name )
      `)
      .order('created_at', { ascending: false });

    if (ownerId) {
      query = query.eq('owner_id', ownerId);
    }

    const { data: metersData, error: metersErr } = await query;

    if (metersErr) {
      console.error('Fetch meters error:', metersErr);
      return NextResponse.json({ error: metersErr.message }, { status: 500 });
    }

    // Attach latest reading and online status (within 15s)
    const now = Date.now();
    const enrichedMeters = await Promise.all(
      (metersData || []).map(async (meter: any) => {
        const lastSeen = meter.last_seen_at ? new Date(meter.last_seen_at).getTime() : 0;
        const isOnline = now - lastSeen < 45000;

        const { data: latestReading } = await supabase
          .from('readings')
          .select('voltage, current, power, energy_kwh, recorded_at')
          .eq('meter_id', meter.id)
          .order('recorded_at', { ascending: false })
          .limit(1)
          .maybeSingle();

        return {
          id: meter.id,
          device_id: meter.device_id,
          serial_number: meter.serial_number,
          qr_code: meter.serial_number || meter.device_id,
          label: meter.label,
          balance: meter.balance,
          relay_on: meter.relay_on,
          last_seen_at: meter.last_seen_at,
          is_online: isOnline,
          property_name: meter.properties?.name || 'Main Property',
          latest_reading: latestReading || null
        };
      })
    );

    return NextResponse.json({
      organization: {
        name: 'Lampitex Smart Meter Network'
      },
      meters: enrichedMeters
    });
  } catch (error: any) {
    console.error('API /meters error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}

export async function DELETE(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const meterId = searchParams.get('id');

    if (!meterId) {
      return NextResponse.json({ error: 'Meter ID required' }, { status: 400 });
    }

    // Get serial number to unclaim product in registry
    const { data: meter } = await supabase
      .from('meters')
      .select('serial_number')
      .eq('id', meterId)
      .single();

    if (meter?.serial_number) {
      await supabase
        .from('product_registry')
        .update({ is_claimed: false, claimed_at: null })
        .eq('serial_number', meter.serial_number);
    }

    const { error } = await supabase.from('meters').delete().eq('id', meterId);

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ success: true });
  } catch (err: any) {
    console.error('API /meters DELETE error:', err);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
