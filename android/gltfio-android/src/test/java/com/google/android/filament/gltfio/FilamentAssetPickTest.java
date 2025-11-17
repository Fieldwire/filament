package com.google.android.filament.gltfio;

import com.google.android.filament.Engine;
import org.junit.Test;

import static org.junit.Assert.*;

public class FilamentAssetPickTest {
    @Test(expected = IllegalArgumentException.class)
    public void pickThrowsOnShortOrigin() {
        // We cannot instantiate a real FilamentAsset without native context; test only Java validation.
        FilamentAsset asset = new FilamentAsset((Engine) null, 0L);
        asset.pick(new float[]{0f, 1f}, new float[]{0f, 0f, 1f});
    }

    @Test(expected = IllegalArgumentException.class)
    public void pickThrowsOnShortDirection() {
        FilamentAsset asset = new FilamentAsset((Engine) null, 0L);
        asset.pick(new float[]{0f, 0f, 0f}, new float[]{1f});
    }

    @Test
    public void pickReturnsNullWhenNativeIsZero() {
        FilamentAsset asset = new FilamentAsset((Engine) null, 0L);
        assertNull(asset.pick(new float[]{0f,0f,0f}, new float[]{0f,0f,1f}));
    }
}

